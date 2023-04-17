#include <utility>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <cstring>
#include <csignal>

static struct termios ORIGINAL_TERMIOS;
sig_atomic_t endProgram = 0;

enum COLOR {
    NONE = 0,
    FG_BLACK = 30,
    FG_RED = 31,
    FG_GREEN = 32,
    FG_YELLOW = 33,
    FG_BLUE = 34,
    FG_MAGENTA = 35,
    FG_CYAN = 36,
    FG_WHITE = 37,

    BG_BLACK = 40,
    BG_RED = 41,
    BG_GREEN = 42,
    BG_YELLOW = 43,
    BG_BLUE = 44,
    BG_MAGENTA = 45,
    BG_CYAN = 46,
    BG_WHITE = 47
};

enum STYLE {
    SNONE = 0,
    BOLD = 1,
    FAINT = 3,
    ITALIC = 4,
    UNDERLINE = 5
};

struct ScreenChar {
    //    ScreenChar(char ch): c(ch) {};
    char c{};
    COLOR fgColor = NONE, bgColor = NONE;
    STYLE style = SNONE;

};

namespace aen {
    class ASCIIEngine
    {
    private:
        std::vector<std::vector<ScreenChar>> m_screenBuffer;
        std::string m_profileID, m_appName;
        std::atomic<bool> m_bAtomRunning{};
        int m_width{},m_height{};
    
    public:
        ASCIIEngine() = default;
        ~ASCIIEngine(){
            SetFontSize(12);
            // Show cursor
            std::cout << "\e[?25h";
            //Set console size
            std::cout << "\e[8;" << 24 << ";" << 80 << "t";
            system("clear");
        }

        void ConstructConsole(int width, int height, const std::string& appName = "",  int fontSize = 12, const std::string& profileID = {}){
            signal(SIGINT, [](int signal){ endProgram = 1; });
            unsigned short screenSize[2];
            m_width = width;
            m_height = height;
            if (GetScreenSize(screenSize)) {
                if (fontSize*width > screenSize[0]) m_width = screenSize[0]/(fontSize);
                if (2*fontSize*height > screenSize[1]) m_height = screenSize[1]/(2*fontSize);
            }
            
            m_profileID = profileID;
            m_appName = appName;

            m_screenBuffer.resize(m_height);
            for (auto& line: m_screenBuffer){ 
                line.resize(m_width);
                for (auto& sc: line)
                    sc.c = ' ';
            }
            if (!profileID.empty()) SetFontSize(fontSize);
            system("clear");
            // Hide cursor
            std::cout << "\e[?25l";
            // Set console size
            std::cout << "\e[8;" << m_height + 1 << ";" << m_width << "t";
        }

        void Draw(int x, int y, char c = '.', COLOR fgColor = NONE, COLOR bgColor = NONE, STYLE style = SNONE){
            if (x >= 0 && x < m_width && y >= 0 && y < m_height){
                m_screenBuffer[y][x].c = c;
                m_screenBuffer[y][x].fgColor = fgColor;
                m_screenBuffer[y][x].bgColor = bgColor;
                m_screenBuffer[y][x].style = style;
            }
        }
        
        void Fill(int x0, int y0,int x1,int y1, COLOR bgColor = NONE){
            Clip(x0, y0); 
            Clip(x1, y1);
            for (int i = y0; i < y1; i++)
                for (int j = x0; j < x1; j++){
                    m_screenBuffer[i][j].c = ' ';
                    m_screenBuffer[i][j].bgColor = bgColor;
                }
        }
        
        void FillScreen(COLOR bgColor = NONE){
            for (int i = 0; i < m_height; i++)
                for (int j = 0; j < m_width; j++){
                    m_screenBuffer[i][j].c = ' ';
                    m_screenBuffer[i][j].bgColor = bgColor;
                }
        }
        
        void DrawString(int x, int y, const std::string& s, COLOR fgColor = NONE, COLOR bgColor = NONE){
            for (int i = 0; i < int(s.length()); i++){
                int nx = x + i;
                Clip(nx, y);
                Draw(nx, y, s[i], fgColor, bgColor);
            }
        }

        void Run(){
            m_bAtomRunning = true;
            std::thread t(&ASCIIEngine::GameThread, this);

            t.join();
        }
        
        
    private:
        void Clip(int& x, int& y) const{
            if (x < 0) x = 0;
            if (x >= m_width) x = m_width;
            if (y < 0) y = 0;
            if (y >= m_height) y = m_height;
        }
        
        static void MoveCursor(int row, int col){
            std::cout << "\033[" << row << ";" << col << "H";
        }

        void SetFontSize(int fontSize){
            std::string s = "gsettings set org.gnome.Terminal.Legacy.Profile:/org/gnome/terminal/legacy/profiles:/:" + m_profileID + 
            "/ font 'Monospace Regular " + std::to_string(fontSize) + "'";
            system(s.c_str());
        }

        void DrawScreen(){
            MoveCursor(1, 1);
            for (auto& line: m_screenBuffer){
                for (auto& sc: line){
                    if (sc.bgColor) std::cout << "\033[" << sc.bgColor << "m";
                    if (sc.fgColor) std::cout << "\033[" << sc.fgColor << "m";
                    if (sc.style) std::cout << "\033[" << sc.style << "m";
                    std::cout << sc.c << "\033[0m";
                }
                std::cout << std::endl;
            }
            MoveCursor(1, 1);
        }

        
        void GameThread(){
            if (!OnCreate()) m_bAtomRunning = false;

            auto t1 = std::chrono::system_clock::now();
            auto t2 = std::chrono::system_clock::now();

            while (m_bAtomRunning && !endProgram){
                // Handle delta time
                t2 = std::chrono::system_clock::now();
                std::chrono::duration<float, std::chrono::seconds::period> elapsedTime = t2 - t1;
                t1 = t2;
                float fDelta = elapsedTime.count();

                char cKey = GetKey();
                if (cKey != '\0') system("clear");

                if (!GameLoop(fDelta, cKey)) m_bAtomRunning = false;

                std::cout << "\033]0;ASCII Engine - " << m_appName << " - FPS: "<< 1.0f/fDelta <<"\007" << std::flush;
                DrawScreen();
            }

            OnDestroy();
        }

        
    static void TerminalReset(){
        tcsetattr(0, TCSANOW, &ORIGINAL_TERMIOS);

    }

    static void TerminalSetRaw(){
        struct termios newTermios{};

        tcgetattr(0, &ORIGINAL_TERMIOS);
        memcpy(&newTermios, &ORIGINAL_TERMIOS, sizeof(newTermios));

        atexit(TerminalReset);
        cfmakeraw(&newTermios);
        tcsetattr(0, TCSANOW, &newTermios);
    }

    static char GetKey(){
        TerminalSetRaw();
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        char cs[5];
        cs[0] = '\0';
        size_t r = 1;

        if (select(1, &fds, nullptr, nullptr, &tv) > 0)
            r = read(0, &cs, sizeof(cs));

        TerminalReset();
        return cs[r - 1];
    }

    static bool GetScreenSize(unsigned short size[2]) {
        char *array[8];
        char screenSize[64];
        char* token;

        FILE *cmd = popen("xdpyinfo | awk '/dimensions/ {print $2}'", "r");

        if (!cmd)
            return false;

        while (fgets(screenSize, sizeof(screenSize), cmd) != nullptr);
        pclose(cmd);

        token = strtok(screenSize, "x\n");

        if (!token)
            return false;
        for (unsigned short i = 0; token != nullptr; ++i) {
            array[i] = token;
            token = strtok(nullptr, "x\n");
        }
        size[0] = strtol(array[0], nullptr, 10);
        size[1] = strtol(array[1], nullptr, 10);

        return true;
    }

    public:
        virtual bool GameLoop(float fDelta, char cKey) = 0;
        virtual bool OnCreate() = 0;
        virtual bool OnDestroy() {return true; };

        [[nodiscard]] int getGameWidth() const{ return m_width; }
        [[nodiscard]] int getGameHeight() const{ return m_height; }
        void setAppName(std::string name){ m_appName = std::move(name); }
    };

    
}

