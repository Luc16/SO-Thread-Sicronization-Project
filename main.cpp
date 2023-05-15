#include "ASCIIEngine.h"
#include <random>
#include <functional>
#include <pthread.h>
#include "lightswitch.h"

#define THREAD_FRAME_TIME 20000//30000

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
                    if (mvChar->x >= width - 4) {
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
                if (mvChar->x - 4 <= 5) {
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

    moveCharInThread(&obj->mvChar, x, y);

    for (ThreadObject* cur = obj->prev; cur != nullptr; cur = cur->prev){
        cur->mvChar.y--;
    }

    (*obj->count)--;
    pthread_mutex_unlock(obj->countMutex);
}

void* searcher_thread(void* threadInfo) {
    auto* searcherInfo = (SearchThreadInfo*) threadInfo;

    moveCharInThread(&searcherInfo->mvChar, 3, 0);

    sem_wait(searcherInfo->noDeleter);
    sem_post(searcherInfo->noDeleter);

    searcherInfo->searcherSwitch->lock(searcherInfo->noSearcher);

    moveLine(searcherInfo, 2, -2);

    traverseList(&searcherInfo->mvChar, searcherInfo->list, &searcherInfo->c, searcherInfo->width,
                 [&searcherInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl) {scharListEl->schar.fgColor = searcherInfo->mvChar.schar.fgColor;});

    searcherInfo->searcherSwitch->unlock(searcherInfo->noSearcher);
    return nullptr;
}

void* inserter_thread(void* threadInfo) {
    auto* inserterInfo = (InsertThreadInfo*) threadInfo;

    moveCharInThread(&inserterInfo->mvChar, 2, 0);

    pthread_mutex_lock(inserterInfo->mutex);

    sem_wait(inserterInfo->noDeleter);
    sem_post(inserterInfo->noDeleter);
    sem_wait(inserterInfo->noInserter);

    moveLine(inserterInfo, 3, -2);

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

    moveCharInThread(&deleterInfo->mvChar, 1, 0);

    deleterInfo->deleterSwitch->lock(deleterInfo->noDeleter);
    sem_wait(deleterInfo->noSearcher);
    sem_wait(deleterInfo->noInserter);

    moveLine(deleterInfo, 4, -2);

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
    ScreenCharList* list{};

    SearchThreadInfo* searcherList{};
    LightSwitch searcherLightSwitch{};
    sem_t noSearcher{};
    pthread_mutex_t searcherCountMutex = PTHREAD_MUTEX_INITIALIZER;
    int searcherCount = 0;

    InsertThreadInfo* inserterList{};
    pthread_mutex_t inserterMutex = PTHREAD_MUTEX_INITIALIZER;
//    LightSwitch inserterLightSwitch{};
    sem_t noInserter{};
    pthread_mutex_t inserterCountMutex = PTHREAD_MUTEX_INITIALIZER;
    int inserterCount = 0;

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
        insertListElement('a');
        insertListElement('d');
        insertListElement('n');
        insertListElement('i');
        insertListElement('l');
        insertListElement('a');
        insertListElement('n');
        for (uint32_t i = 0; i < 100; i++){
            if (i == 80){
                insertListElement('c');
                continue;
            }
            insertListElement('a');

        }

        sem_init(&noSearcher, 1, 1);
        sem_init(&noInserter, 1, 1);
        sem_init(&noDeleter, 1, 1);

        return true;
    }

    bool GameLoop(float fDelta, char cKey) override {
        FillScreen();

        switch (cKey)
        {
            case 's':
                createSearcher();
                break;
            case 'i':
                createInserter();
                break;
            case 'd':
                createDeleter();
                break;
            case 27: // ESC
                return false;
            default:
                break;
        }

        drawList();
        drawObjects();

        return true;
    }

    void drawList(){
        int x = 5;
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

                    if (x + 5 >= getGameWidth() - 3) {
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

                    if (x - 5 <= 3) {
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

                    free(temp);
                    continue;
                }

                Draw(current->mvChar.x, current->mvChar.y, current->mvChar.schar.c, current->mvChar.schar.fgColor);
//                Draw(current->mvChar.x + 1, current->mvChar.y, current->c, current->mvChar.schar.fgColor);
                current = current->next;
            }
        }
    }

    void createSearcher() {
        pthread_mutex_lock(&searcherCountMutex);
        searcherCount++;

        auto* info = (SearchThreadInfo*) malloc(sizeof(SearchThreadInfo));
        info->mvChar = {0, 5 + searcherCount, {'S', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = 'l';
        info->width = getGameWidth();
        info->height = getGameHeight();

        info->next = searcherList;
        info->prev = nullptr;
        if (searcherList != nullptr) searcherList->prev = info;
        searcherList = info;

        info->searcherSwitch = &searcherLightSwitch;
        info->noSearcher = &noSearcher;
        info->noDeleter = &noDeleter;

        info->countMutex = &searcherCountMutex;
        info->count = &searcherCount;

        pthread_create(&info->thread, nullptr, &searcher_thread, (void *) info);
        pthread_mutex_unlock(&searcherCountMutex);
    }

    void createInserter(){
        pthread_mutex_lock(&inserterCountMutex);
        inserterCount++;

        auto* info = (InsertThreadInfo*) malloc(sizeof(InsertThreadInfo));
        info->mvChar = {0, 5 + inserterCount, {'I', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = 'c';
        info->width = getGameWidth();
        info->height = getGameHeight();
        info->a = char('a' + randomInt(0, 25));

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

    void createDeleter() {
        pthread_mutex_lock(&deleterCountMutex);
        deleterCount++;

        auto* info = (DeleterThreadInfo*) malloc(sizeof(DeleterThreadInfo));
        info->mvChar = {0, 5 + deleterCount, {'D', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = 'n';
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
//        ThreadObject* lists[3] = {searcherList, inserterList, deleterList};
//        for (auto& objList : lists) {
//            ThreadObject *prev;
//            for (ThreadObject *current = objList; current != nullptr;) {
//                prev = current;
//                current = current->next;
//                free(prev);
//            }
//        }
        freeList();
        sem_destroy(&noSearcher);
        sem_destroy(&noInserter);
        sem_destroy(&noDeleter);

        pthread_mutex_destroy(&searcherCountMutex);
        pthread_mutex_destroy(&inserterCountMutex);
        pthread_mutex_destroy(&deleterCountMutex);
        return true;
    }

};

int main(){
    SearchInsertDeleteDemo engine;
    engine.ConstructConsole(160, 45, "");
    engine.Run();

 }






