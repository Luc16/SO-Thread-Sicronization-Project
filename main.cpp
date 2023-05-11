#include "ASCIIEngine.h"
#include <string>
#include <random>
#include <functional>
#include <pthread.h>
#include "lightswitch.h"

#define THREAD_FRAME_TIME 30000

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
    ScreenCharList** list{};
    ThreadObject* next{};
};

struct SearchThreadInfo: public ThreadObject{
    LightSwitch* searcherSwitch{};
    sem_t* noSearcher{};
    sem_t* noDeleter{};
};

struct InsertThreadInfo: public ThreadObject {
    LightSwitch* inserterSwitch{};
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

void traverseList(MovingChar* mvChar, ScreenCharList** list, char charToFind, int width, const std::function<void(ScreenCharList*, ScreenCharList*)>& handle) {
    int state = 0, prevState = 0;
    ScreenCharList* prev = nullptr;
    for (ScreenCharList* current = *list; current != nullptr; current = current->next) {
        auto& schar = current->schar;
        if (schar.c == charToFind) {
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
        prev = current;
    }
}

void* searcher_thread(void* threadInfo) {
    auto* searcherInfo = (SearchThreadInfo*) threadInfo;

    sem_wait(searcherInfo->noDeleter);
    sem_post(searcherInfo->noDeleter);
    searcherInfo->searcherSwitch->lock(searcherInfo->noSearcher);

    traverseList(&searcherInfo->mvChar, searcherInfo->list, searcherInfo->c, searcherInfo->width,
                 [&searcherInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl) {scharListEl->schar.fgColor = searcherInfo->mvChar.schar.fgColor;});
    searcherInfo->c = '\0';

    searcherInfo->searcherSwitch->unlock(searcherInfo->noSearcher);
    return nullptr;
}

void* inserter_thread(void* threadInfo) {
    auto* inserterInfo = (InsertThreadInfo*) threadInfo;

    sem_wait(inserterInfo->noDeleter);
    sem_post(inserterInfo->noDeleter);

    inserterInfo->inserterSwitch->lock(inserterInfo->noInserter);

    pthread_mutex_lock(inserterInfo->mutex);

    traverseList(&inserterInfo->mvChar, inserterInfo->list, inserterInfo->c, inserterInfo->width,
                 [&inserterInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl){
        auto* newElement = (ScreenCharList*) malloc(sizeof(ScreenCharList));
        newElement->schar = {inserterInfo->a, inserterInfo->mvChar.schar.fgColor};
        newElement->next = scharListEl->next;
        scharListEl->next = newElement;
    });
    inserterInfo->c = '\0';

    pthread_mutex_unlock(inserterInfo->mutex);
    inserterInfo->inserterSwitch->unlock(inserterInfo->noInserter);

    return nullptr;
}

void* deleter_thread(void* threadInfo) {
    auto* deleterInfo = (DeleterThreadInfo*) threadInfo;
    deleterInfo->deleterSwitch->lock(deleterInfo->noDeleter);
    sem_wait(deleterInfo->noSearcher);
    sem_wait(deleterInfo->noInserter);

    traverseList(&deleterInfo->mvChar, deleterInfo->list, deleterInfo->c, deleterInfo->width,
                 [&deleterInfo](ScreenCharList* scharListEl, ScreenCharList* prevEl) {
        if (prevEl == nullptr)
            *deleterInfo->list = scharListEl->next;
        else
            prevEl->next = scharListEl->next;
        free(scharListEl);
    });
    deleterInfo->c = '\0';

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

    InsertThreadInfo* inserterList{};
    pthread_mutex_t inserterMutex{};
    LightSwitch inserterLightSwitch{};
    sem_t noInserter{};

    DeleterThreadInfo* deleterList{};
    LightSwitch deleterLightSwitch{};
    sem_t noDeleter{};



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

//        if (cKey != '\0' && cKey != '\t' && cKey != '\n') list.push_front({cKey});
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
            case 27:
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

    void drawObjects(){
        ThreadObject* lists[3] = {searcherList, inserterList, deleterList};
        for (int i = 0; i < 3; i++) {
            ThreadObject* prev = nullptr;
            for (ThreadObject* current = lists[i]; current != nullptr;){
                // delete in case thread is finished
                if (current->c == '\0') {
                    ThreadObject* temp = current;
                    current = current->next;
                    if (prev == nullptr) {
                        if(i == 0) searcherList = static_cast<SearchThreadInfo *>(current);
                        else if(i == 1) inserterList = static_cast<InsertThreadInfo *>(current);
                        else if(i == 2) deleterList = static_cast<DeleterThreadInfo *>(current);
                        lists[i] = current;
                    }
                    else prev->next = current;
                    free(temp);
                    continue;
                }
                Draw(current->mvChar.x, current->mvChar.y, current->mvChar.schar.c, current->mvChar.schar.fgColor);
                prev = current;
                current = current->next;
            }
        }
    }

    void createSearcher() {

        auto* info = (SearchThreadInfo*) malloc(sizeof(SearchThreadInfo));
        info->mvChar = {5, 4, {'S', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = 'l';
        info->width = getGameWidth();
        info->height = getGameHeight();
        info->next = searcherList;
        searcherList = info;

        info->searcherSwitch = &searcherLightSwitch;
        info->noSearcher = &noSearcher;
        info->noDeleter = &noDeleter;

        pthread_create(&info->thread, nullptr, &searcher_thread, (void *) info);

    }

    void createInserter() {

        auto* info = (InsertThreadInfo*) malloc(sizeof(InsertThreadInfo));
        info->mvChar = {5, 4, {'I', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = 'c';
        info->width = getGameWidth();
        info->height = getGameHeight();
        info->next = inserterList;
        info->mutex = &inserterMutex;
        info->a = char('a' + randomInt(0, 25));
        inserterList = info;

        info->inserterSwitch = &inserterLightSwitch;
        info->noInserter = &noInserter;
        info->noDeleter = &noDeleter;

        pthread_create(&info->thread, nullptr, &inserter_thread, (void *) info);

    }

    void createDeleter() {
        auto* info = (DeleterThreadInfo*) malloc(sizeof(DeleterThreadInfo));
        info->mvChar = {5, 4, {'D', static_cast<COLOR>(randomInt(31, 36))}};
        info->list = &list;
        info->c = 'n';
        info->width = getGameWidth();
        info->height = getGameHeight();
        info->next = deleterList;
        deleterList = info;

        info->deleterSwitch = &deleterLightSwitch;
        info->noSearcher = &noSearcher;
        info->noInserter = &noInserter;
        info->noDeleter = &noDeleter;


        info->noDeleter = &noDeleter;

        pthread_create(&info->thread, nullptr, &deleter_thread, (void *) info);

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
        ThreadObject* lists[3] = {searcherList, inserterList, deleterList};
        for (auto& objList : lists) {
            ThreadObject *prev;
            for (ThreadObject *current = objList; current != nullptr;) {
                prev = current;
                current = current->next;
                free(prev);
            }
        }
        freeList();
        sem_destroy(&noDeleter);
        return true;
    }


};

int main(){
//    std::string profileID = "f5f27596-afd0-420a-8aae-8220491dc05b";
    SearchInsertDeleteDemo engine;
    engine.ConstructConsole(160, 45, "");
    engine.Run();

 }






