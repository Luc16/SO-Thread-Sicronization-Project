#include "ASCIIEngine.h"
#include <string>
#include <random>
#include <forward_list>
#include <pthread.h>
#include "lightswitch.h"

#define THREAD_FRAME_TIME 30000

struct MovingChar {
    int x{}, y{};
    ScreenChar schar{};
};

struct SearchThreadInfo {
    pthread_t thread{};
    // LightSwitch& searcherSwitch;
    // sem_t& noSearcher;
    MovingChar mvChar{};
    std::forward_list<ScreenChar>* list{};
    char c{};
    int width{}, height{};
    SearchThreadInfo* next{};

};

void search(MovingChar* mvChar, std::forward_list<ScreenChar>* list, char c, int width, int height) {
    int state = 0, prevState = 0;

    for (auto & schar : *list) {
        if (schar.c == c) {
            schar.fgColor = mvChar->schar.fgColor;
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
    }
}

void* searcher_thread(void * threadInfo) {
    auto* searcherInfo = (SearchThreadInfo*) threadInfo;
    // searcherInfo->searcherSwitch.lock(searcherInfo->noSearcher);

    search(&searcherInfo->mvChar, searcherInfo->list, searcherInfo->c, searcherInfo->width, searcherInfo->height);

    // searcherInfo->searcherSwitch.unlock(searcherInfo->noSearcher);
    return nullptr;
}

class SearchInsertDeleteDemo: public aen::ASCIIEngine {
    std::forward_list<ScreenChar> list;
    SearchThreadInfo* searcherList{};


protected:

    void DrawList(){
        int x = 5;
        int y = 5;

        int state = 0, prevState = 0;

        for (auto it = list.begin(); it != list.end(); ++it) {
            const auto& item = *it;
            switch (state) {
                case 0:
                    Draw(x++, y, item.c, item.fgColor, item.bgColor, item.style);
                    if (std::next(it, 1) != list.end()) {
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
                    Draw(x, y++, item.c, item.fgColor, item.bgColor, item.style);
                    if (std::next(it, 1) != list.end()) {
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
                    Draw(--x, y, item.c, item.fgColor, item.bgColor, item.style);
                    if (std::next(it, 1) != list.end()) {
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

    void DrawObjects() {
        for (SearchThreadInfo* current = searcherList; current != nullptr; current = current->next){
            Draw(current->mvChar.x, current->mvChar.y, current->mvChar.schar.c, current->mvChar.schar.fgColor);
        }
    }

    void createSearcher() {
        auto searcher = (MovingChar *) malloc(sizeof(MovingChar));
        searcher->x = 5;
        searcher->y = 4;
        searcher->schar = {'S', FG_GREEN};

        auto* info = (SearchThreadInfo*) malloc(sizeof(SearchThreadInfo));
        info->mvChar = {5, 4, {'S', FG_GREEN}};
        info->list = &list;
        info->c = 'l';
        info->width = getGameWidth();
        info->height = getGameHeight();
        info->next = searcherList;
        searcherList = info;


        pthread_create(&info->thread, nullptr, &searcher_thread, (void *) info);

    }

    bool OnCreate() override{
        list.push_front({'a'});
        list.push_front({'d'});
        list.push_front({'n'});
        list.push_front({'i'});
        list.push_front({'l'});
        list.push_front({'a'});
        list.push_front({'n'});
        for (uint32_t i = 0; i < 100; i++){
            if (i == 38){
                list.push_front({'c'});
                continue;
            }
            list.push_front({'a'});

        }

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
           case 27:
                return false;
            default:
                break;
        }


        DrawList();
        DrawObjects();

        return true;
    }

    bool OnDestroy() override {
        return true;
    }


};

int main(){
    std::string profileID = "f5f27596-afd0-420a-8aae-8220491dc05b";
    SearchInsertDeleteDemo engine;
    engine.ConstructConsole(160, 45, "");
    engine.Run();

 }






