#include "ASCIIEngine.h"
#include <string>
#include <random>
#include <forward_list>

class SearchInsertDeleteDemo: public aen::ASCIIEngine {
    std::forward_list<ScreenChar> list;


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

    bool OnCreate() override{
        list.push_front({'a'});
        list.push_front({'d'});
        list.push_front({'n'});
        list.push_front({'i'});
        list.push_front({'l'});
        list.push_front({'a'});
        list.push_front({'n'});
        for (uint32_t i = 0; i < 100; i++){
            list.push_front({'a'});

        }
        return true;
    }

    bool GameLoop(float fDelta, char cKey) override{
        FillScreen();

        if (cKey != '\0') list.push_front({cKey});
        switch (cKey)
        {
//            case 'a':
//                break;
            case 'l':
                break;
           case 27:
                return false;
            default:
                break;
        }


        DrawList();

        return true;
    }

};

int main(){
//    std::string profileID = "f5f27596-afd0-420a-8aae-8220491dc05b";
    SearchInsertDeleteDemo engine;
    engine.ConstructConsole(600, 150, "");
    engine.Run();
}






