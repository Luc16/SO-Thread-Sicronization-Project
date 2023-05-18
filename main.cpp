#include "ASCIIEngine.h"
#include <random>
#include <functional>
#include <pthread.h>
#include "lightswitch.h"
#include <string>

int THREAD_FRAME_TIME = 20000;

struct ScreenCharList {
    ScreenChar schar{};
    ScreenCharList* next{};
};

struct MovingChar {
    int x{}, y{};
    ScreenChar schar{};
};

struct ThreadObject {
    pthread_t thread{};
    MovingChar mvChar;
    char c{};
    int width{}, height{};
    int* count{};
    pthread_mutex_t* countMutex{};
    ScreenCharList** list{};
    ThreadObject* next{};
    ThreadObject* prev{};
};

struct SearchThreadInfo: public ThreadObject{
    LightSwitch* searcherSwitch{};
    sem_t* noSearcher{};
    sem_t* noDeleter{};
    pthread_mutex_t* searcherSyncLineMutex{};

};

struct InsertThreadInfo: public ThreadObject {
//    LightSwitch* inserterSwitch{};
    sem_t* noInserter{};
    sem_t* noDeleter{};
    pthread_mutex_t* mutex{};
    char a{};
};

struct DeleterThreadInfo: public ThreadObject{
    LightSwitch* deleterSwitch{};
    sem_t* noSearcher{};
    sem_t* noInserter{};
    sem_t* noDeleter{};
};

inline int randomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis_int(min, max);
    return dis_int(gen);
}

inline char randomLetter() {
    return char('a' + randomInt(0, 25));
}

void traverseList(MovingChar* mvChar, ScreenCharList** list, char* charToFind, int width, const std::function<void(ScreenCharList*, ScreenCharList*)>& handle) {
    int count = 0, listCount;
    int state = 0, prevState = 0;
    ScreenCharList* prev = nullptr;
    for (ScreenCharList* current = *list; current != nullptr;) {
        count++;
        auto& schar = current->schar;
        if (schar.c == *charToFind) {
            handle(current, prev);
            break;
        }
        switch (state) {
            case 0:
                for (int i = 0; i < ((prevState == 2) ? 4: 5); i++) {
                    usleep(THREAD_FRAME_TIME);
                    mvChar->x += 1;
                    if (mvChar->x >= width - 19) {
                        state = 1;
                        for (int j = 0; j < 5; j++) {
                            usleep(THREAD_FRAME_TIME);
                            mvChar->y += 1;
                        }
                        usleep(THREAD_FRAME_TIME);
                        mvChar->x -= 1;
                        break;
                    }
                }
                prevState = 0;

                break;
            case 1:
                for (int i = 0; i < 5; i++) {
                    usleep(THREAD_FRAME_TIME);
                    mvChar->x -= 1;
                }
                if (mvChar->x - 20 <= 4) {
                    state = 2;
                    mvChar->x += 1;
                }
                break;
            case 2:
                usleep(THREAD_FRAME_TIME);
                mvChar->y += 1;
                state = 0;
                prevState = 2;

            default:
                break;
        }
        // verifica se alguém foi adicionado antes da posição atual, e se foi, anda um frame extra
        listCount = 1;
        for (ScreenCharList* cur = *list; cur != current; cur = cur->next) listCount++;
        if (listCount == count) {
            prev = current;
            current = current->next;
        }

    }
    usleep(2*THREAD_FRAME_TIME);
    *charToFind = '\0';
}

void moveCharInThread(MovingChar* mvChar, int x, int y){
    int add = (y > 0) ? 1 : -1;
    for (int i = 0; i < add*y; i++) {
        mvChar->y += add;
        usleep(THREAD_FRAME_TIME);
    }
    add = (x > 0) ? 1 : -1;
    for (int i = 0; i < add*x; i++) {
        mvChar->x += add;
        usleep(THREAD_FRAME_TIME);
    }
}

// move a fila de threads de um certo tipo que estão esperando
void moveLine(ThreadObject* obj, int x, int y) {
    pthread_mutex_lock(obj->countMutex);

    for (ThreadObject* cur = obj->prev; cur != nullptr; cur = cur->prev){
        cur->mvChar.y--;
    }

    (*obj->count)--;
    pthread_mutex_unlock(obj->countMutex);

    moveCharInThread(&obj->mvChar, x, y);
}

void* searcher_thread(void* threadInfo) {
    auto* searcherInfo = (SearchThreadInfo*) threadInfo;
    moveCharInThread(&searcherInfo->mvChar, 12, 0);


    pthread_mutex_lock(searcherInfo->searcherSyncLineMutex);

    sem_wait(searcherInfo->noDeleter);
    sem_post(searcherInfo->noDeleter);

    searcherInfo->searcherSwitch->lock(searcherInfo->noSearcher);


    moveLine(searcherInfo, 8, -2);
    pthread_mutex_unlock(searcherInfo->searcherSyncLineMutex);

    traverseList(&searcherInfo->mvChar, searcherInfo->list, &searcherInfo->c, searcherInfo->width,
                 [&searcherInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl) {scharListEl->schar.fgColor = searcherInfo->mvChar.schar.fgColor;});

    searcherInfo->searcherSwitch->unlock(searcherInfo->noSearcher);
    return nullptr;
}

void* inserter_thread(void* threadInfo) {
    auto* inserterInfo = (InsertThreadInfo*) threadInfo;

    moveCharInThread(&inserterInfo->mvChar, 7, 0);

    pthread_mutex_lock(inserterInfo->mutex);

    sem_wait(inserterInfo->noDeleter);
    sem_post(inserterInfo->noDeleter);
    sem_wait(inserterInfo->noInserter);

    moveLine(inserterInfo, 13, -2);

    traverseList(&inserterInfo->mvChar, inserterInfo->list, &inserterInfo->c, inserterInfo->width,
                 [&inserterInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl){
        auto* newElement = (ScreenCharList*) malloc(sizeof(ScreenCharList));
        newElement->schar = {inserterInfo->a, inserterInfo->mvChar.schar.fgColor};
        newElement->next = scharListEl->next;
        if (scharListEl->next != nullptr)
            scharListEl->next = newElement;
    });

    sem_post(inserterInfo->noInserter);
    pthread_mutex_unlock(inserterInfo->mutex);

    return nullptr;
}

void* deleter_thread(void* threadInfo) {
    auto* deleterInfo = (DeleterThreadInfo*) threadInfo;

    moveCharInThread(&deleterInfo->mvChar, 2, 0);

    deleterInfo->deleterSwitch->lock(deleterInfo->noDeleter);
    sem_wait(deleterInfo->noSearcher);
    sem_wait(deleterInfo->noInserter);

    moveLine(deleterInfo, 18, -2);

    traverseList(&deleterInfo->mvChar, deleterInfo->list, &deleterInfo->c, deleterInfo->width,
                 [&deleterInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl) {
        if (prevEl == nullptr)
            *deleterInfo->list = scharListEl->next;
        else
            prevEl->next = scharListEl->next;
        free(scharListEl);
    });

    sem_post(deleterInfo->noSearcher);
    sem_post(deleterInfo->noInserter);
    deleterInfo->deleterSwitch->unlock(deleterInfo->noDeleter);
    return nullptr;
}

class SearchInsertDeleteDemo: public aen::ASCIIEngine {
    enum SelectedInput {SEARCH_TARGET, INSERT_TARGET, INSERT_LETTER, DELETE_TARGET, NONE};
    std::string message = "";

    ScreenCharList* list{};
    int speed = 1000000/THREAD_FRAME_TIME;
    SelectedInput selected = NONE;

    SearchThreadInfo* searcherList{};
    LightSwitch searcherLightSwitch{};
    sem_t noSearcher{};
    pthread_mutex_t searcherCountMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t searcherSyncLineMutex = PTHREAD_MUTEX_INITIALIZER;
    int searcherCount = 0;

    InsertThreadInfo* inserterList{};
    pthread_mutex_t inserterMutex = PTHREAD_MUTEX_INITIALIZER;
//    LightSwitch inserterLightSwitch{};
    sem_t noInserter{};
    pthread_mutex_t inserterCountMutex = PTHREAD_MUTEX_INITIALIZER;
    int inserterCount = 0;
    char inserterLetter = '\0';

    DeleterThreadInfo* deleterList{};
    LightSwitch deleterLightSwitch{};
    sem_t noDeleter{};
    pthread_mutex_t deleterCountMutex = PTHREAD_MUTEX_INITIALIZER;
    int deleterCount = 0;

protected:

    void insertListElement(char c) {
        auto* temp = (ScreenCharList*) malloc(sizeof(ScreenCharList));
        temp->schar = {c};
        temp->next = list;
        list = temp;
    }

    bool OnCreate() override {
        for (uint32_t i = 0; i < 120; i++){
            insertListElement(randomLetter());
        }

        sem_init(&noSearcher, 1, 1);
        sem_init(&noInserter, 1, 1);
        sem_init(&noDeleter, 1, 1);

        return true;
    }

    bool GameLoop(float fDelta, char cKey) override {    
        FillScreen();

        if (cKey != '\0' && (cKey < 'a' || cKey > 'z')){
            selected = NONE;
        }

        char target = '\0';
        char letter = '\0';

        if (cKey != '\0') {
            switch (selected){
                case SEARCH_TARGET:
                    createSearcher(cKey);
                    selected = NONE;
                    message = "Search " + std::string(1, cKey);
                    break;
                case INSERT_LETTER:
                    inserterLetter = cKey;
                    selected = INSERT_TARGET;
                    message = "Insert " + std::string(1, inserterLetter) + " after _";
                    break;             
                case INSERT_TARGET:
                    createInserter(inserterLetter, cKey);
                    selected = NONE;
                    message = "Insert " + std::string(1, inserterLetter) + " after " + std::string(1, cKey);
                    break;
                case DELETE_TARGET:
                    createDeleter(cKey);
                    selected = NONE;
                    message = "Delete " + std::string(1, cKey);
                    break;
                default:
                    switch (cKey) {
                        case 's':
                            target = randomLetter();
                            createSearcher(target);
                            message = "Search " + std::string(1, target);
                            break;
                        case 'i':
                            target = randomLetter();
                            letter = randomLetter();
                            createInserter(letter, target);
                            message = "Insert " + std::string(1, letter) + " after " + std::string(1, target);
                            break;
                        case 'd':
                            target = randomLetter();
                            createDeleter(target);
                            message = "Delete " + std::string(1, target);
                            break;
                        case 'S':
                            selected = SEARCH_TARGET;
                            message = "Search _";
                            break;
                        case 'I':
                            selected = INSERT_LETTER;
                            message = "Insert _ after _ ";
                            break;
                        case 'D':
                            selected = DELETE_TARGET;
                            message = "Delete _";
                            break;
                        case '+':
                            speed = std::min(speed + 10, 1000);
                            THREAD_FRAME_TIME = 1000000/speed;
                            break;
                        case '-':
                            speed = std::max(speed - 10, 10);
                            THREAD_FRAME_TIME = 1000000/speed;
                            break;
                        case 27: // ESC
                            return false;
                        default:
                            break;
                    }
                    break;
            }
        }

        DrawString(20, 1, message);
        DrawString(100, 1, "Speed: " + std::to_string(speed) + " (type + ou - to control)");
        drawList();
        drawObjects();

        return true;
    }

    void drawList(){
        int x = 20;
        int y = 5;

        int state = 0, prevState = 0;

        for (ScreenCharList* current = list; current != nullptr; current = current->next) {
            const auto& schar = current->schar;
            switch (state) {
                case 0:
                    Draw(x++, y, schar.c, schar.fgColor, schar.bgColor, schar.style);
                    if (current->next != nullptr) {
                        DrawString(x, y, " -> ");
                        x += 4;
                    }

                    if (x + 20 >= getGameWidth() - 3) {
                        prevState = 0;
                        state = 1;
                    }
                    break;
                case 1:
                    if (prevState == 2) x--;
                    Draw(x, y++, schar.c, schar.fgColor, schar.bgColor, schar.style);
                    if (current->next != nullptr) {
                        Draw(x, y++, '|');
                        Draw(x, y++, 'V');
                    }
                    state = 2 - prevState;
                    prevState = 1;
                    break;
                case 2:
                    if (prevState == 1) {
                        x++;
                        prevState = 2;
                    }
                    Draw(--x, y, schar.c, schar.fgColor, schar.bgColor, schar.style);
                    if (current->next != nullptr) {
                        x -= 4;
                        DrawString(x, y, " <- ");
                    }

                    if (x - 20 <= 3) {
                        prevState = 2;
                        state = 1;
                    }
                    break;
                default:
                    break;
            }

        }


    }

    void drawObjects() {
        ThreadObject* lists[3] = {searcherList, inserterList, deleterList};
        for (int i = 0; i < 3; i++) {
            for (ThreadObject* current = lists[i]; current != nullptr;){
                // delete in case thread is finished
                if (current->c == '\0') {
                    ThreadObject* temp = current;
                    current = current->next;
                    if (temp->prev == nullptr) {
                        if(i == 0) searcherList = (SearchThreadInfo *)(current);
                        else if(i == 1) inserterList = (InsertThreadInfo *)(current);
                        else if(i == 2) deleterList = (DeleterThreadInfo *)(current);
                    }
                    else temp->prev->next = current;

                    if (current != nullptr) temp->next->prev = temp->prev;
                    pthread_join(temp->thread, nullptr);
                    free(temp);
                    continue;
                }

                Draw(current->mvChar.x, current->mvChar.y, current->mvChar.schar.c, current->mvChar.schar.fgColor);
                char str[4] = {'(', ' ', ')', '\0'};
                str[1] = current->c;
                DrawString(current->mvChar.x + 1, current->mvChar.y, str, current->mvChar.schar.fgColor);
                current = current->next;
            }
        }
    }

    void createSearcher(char c) {
        pthread_mutex_lock(&searcherCountMutex);
        searcherCount++;

        auto* info = (SearchThreadInfo*) malloc(sizeof(SearchThreadInfo));
        info->mvChar = {0, 5 + searcherCount, {'S', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = c;
        info->width = getGameWidth();
        info->height = getGameHeight();

        info->next = searcherList;
        info->prev = nullptr;
        if (searcherList != nullptr) searcherList->prev = info;
        searcherList = info;

        info->searcherSyncLineMutex = &searcherSyncLineMutex;
        info->searcherSwitch = &searcherLightSwitch;
        info->noSearcher = &noSearcher;
        info->noDeleter = &noDeleter;

        info->countMutex = &searcherCountMutex;
        info->count = &searcherCount;

        pthread_create(&info->thread, nullptr, &searcher_thread, (void *) info);
        pthread_mutex_unlock(&searcherCountMutex);
    }

    void createInserter(char a, char c){
        pthread_mutex_lock(&inserterCountMutex);
        inserterCount++;

        auto* info = (InsertThreadInfo*) malloc(sizeof(InsertThreadInfo));
        info->mvChar = {0, 5 + inserterCount, {'I', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = c;
        info->width = getGameWidth();
        info->height = getGameHeight();
        info->a = a;

        info->next = inserterList;
        info->prev = nullptr;
        if (inserterList != nullptr) inserterList->prev = info;
        inserterList = info;

        info->mutex = &inserterMutex;
//        info->inserterSwitch = &inserterLightSwitch;
        info->noInserter = &noInserter;
        info->noDeleter = &noDeleter;

        info->countMutex = &inserterCountMutex;
        info->count = &inserterCount;

        pthread_create(&info->thread, nullptr, &inserter_thread, (void *) info);
        pthread_mutex_unlock(&inserterCountMutex);

    }

    void createDeleter(char c) {
        pthread_mutex_lock(&deleterCountMutex);
        deleterCount++;

        auto* info = (DeleterThreadInfo*) malloc(sizeof(DeleterThreadInfo));
        info->mvChar = {0, 5 + deleterCount, {'D', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = c;
        info->width = getGameWidth();
        info->height = getGameHeight();

        info->next = deleterList;
        info->prev = nullptr;
        if (deleterList != nullptr) deleterList->prev = info;
        deleterList = info;

        info->deleterSwitch = &deleterLightSwitch;
        info->noSearcher = &noSearcher;
        info->noInserter = &noInserter;
        info->noDeleter = &noDeleter;

        info->countMutex = &deleterCountMutex;
        info->count = &deleterCount;


        pthread_create(&info->thread, nullptr, &deleter_thread, (void *) info);
        pthread_mutex_unlock(&deleterCountMutex);

    }

    void freeList() {
        ScreenCharList *prev;
        for (ScreenCharList *current = list; current != nullptr;) {
            prev = current;
            current = current->next;
            free(prev);
        }
    }

    bool OnDestroy() override {
        THREAD_FRAME_TIME = 0;
        ThreadObject* lists[3] = {deleterList, inserterList, searcherList};
        for (auto& objList : lists) {
            for (ThreadObject *current = objList; current != nullptr; current = current->next) {
                pthread_join(current->thread, nullptr);
            }
            ThreadObject *prev;
            for (ThreadObject *current = objList; current != nullptr;) {
                prev = current;
                current = current->next;
                free(prev);
            }
        }


        freeList();
        sem_destroy(&noSearcher);
        sem_destroy(&noInserter);
        sem_destroy(&noDeleter);

        pthread_mutex_destroy(&searcherCountMutex);
        pthread_mutex_destroy(&inserterCountMutex);
        pthread_mutex_destroy(&inserterMutex);
        pthread_mutex_destroy(&deleterCountMutex);
        return true;
    }

};

int main(){
    SearchInsertDeleteDemo engine;
    engine.ConstructConsole(160, 45, "");
    engine.Run();

    std::cout << sizeof(ScreenCharList) << '\n';

 }
