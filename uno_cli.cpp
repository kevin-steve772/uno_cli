#include "console_ui.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <random>
#include <thread>
#include <chrono>
#include <limits>

#ifdef _WIN32
    #include <conio.h>
    inline int my_getch() { return _getch(); }
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>

    static struct termios old_tio, new_tio;

    void init_input() {
        tcgetattr(STDIN_FILENO, &old_tio);
        new_tio = old_tio;
        new_tio.c_lflag &= (~ICANON & ~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    }

    void restore_input() {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    }

    int my_getch() {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) == 1)
            return ch;
        return -1;
    }

    int kbhit() {
        struct timeval tv = { 0L, 0L };
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    }
#endif

using namespace std;

enum CardColor { COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_NONE };
enum CardType { NUMBER, SKIP, REVERSE, DRAW_TWO, WILD, WILD_DRAW_FOUR };

struct Card {
    CardColor color;
    CardType type;
    int number;
    Card(CardColor c = COLOR_NONE, CardType t = NUMBER, int n = 0) : color(c), type(t), number(n) {}
    
    string toString() const {
        if (type == NUMBER) return to_string(number);
        else if (type == SKIP) return "→";
        else if (type == REVERSE) return "↻";
        else if (type == DRAW_TWO) return "+2";
        else if (type == WILD) return "⫶";
        else if (type == WILD_DRAW_FOUR) return "+4";
        return "?";
    }
    
    int getColorCode() const {
        switch (color) {
            case COLOR_RED:    return RED;
            case COLOR_GREEN:  return GREEN;
            case COLOR_BLUE:   return BLUE;
            case COLOR_YELLOW: return YELLOW;
            default:           return DEFAULT;
        }
    }
    
    bool operator==(const Card& other) const {
        return color == other.color && type == other.type && number == other.number;
    }
};

class Player {
public:
    string name;
    bool isAI;
    vector<Card> hand;
    Player(const string& n, bool ai) : name(n), isAI(ai) {}
    void addCard(const Card& card) { hand.push_back(card); }
    void removeCard(int index) { hand.erase(hand.begin() + index); }
    int getHandSize() const { return hand.size(); }
};

class UNOGame {
private:
    vector<Player> players;
    vector<Card> drawPile, discardPile;
    int currentPlayer, direction;
    bool gameOver;
    int winnerIndex;
    int termHeight, termWidth;
    int lastCurrentPlayer;
    vector<int> lastHandSizes;
    Card lastTopCard;
    int lastDrawPileSize;
    bool firstDraw;
    int selectedCardIndex;
    bool waitingForHumanInput;

public:
    UNOGame();
    void run();

private:
    void setupPlayers();
    void initGame();
    void createDeck();
    void shuffleDeck();
    void dealCards();
    void setupDiscardPile();
    void reshuffleDiscard();
    Card drawCard();
    bool isLegalPlay(const Card& played, const Card& top) const;
    void applyCardEffect(const Card& card);
    int getNextPlayer() const;
    int getNextPlayerAfterSkip(int skipped);
    CardColor chooseWildColor();
    bool playTurn();
    void drawFullUI();
    void updateUI();
    void drawCardBack(int x, int y);
    void drawCardFace(const Card& card, int x, int y, bool selected = false, bool legal = false);
    void drawPlayerAreaStatic();
    void drawCenterPileStatic();
    void drawBorder();
    void drawMessage(const string& msg, int color = YELLOW);
    void clearMessageArea();
    void handleHumanTurn();
    int findFirstLegalCardIndex();
    void moveSelectionLeft();
    void moveSelectionRight();
    string centerString(const string& sym) const {
        if (sym.size() == 1) return string("  ") + sym + string("  ");
        else return string(" ") + sym + string("  ");
    }
};
UNOGame::UNOGame() : lastCurrentPlayer(-1), lastDrawPileSize(-1), firstDraw(true), selectedCardIndex(0), waitingForHumanInput(false) {
    setupPlayers();
    initGame();
}

void UNOGame::setupPlayers() {
    string playerName;
    clrtxt("请输入你的名字: ", CYAN);
    getline(cin, playerName);
    if (playerName.empty()) playerName = "玩家";
    players.push_back(Player(playerName, false));
    
    int aiCount;
    while (true) {
        clrtxt("请输入AI玩家数量 (1~3): ", CYAN);
        if (cin >> aiCount && aiCount >= 1 && aiCount <= 3) break;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        clrtxt("输入无效，请输入1到3之间的整数。\n", RED);
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
    for (int i = 1; i <= aiCount; ++i)
        players.push_back(Player(string("AI_") + to_string(i), true));
}

void UNOGame::initGame() {
    createDeck();
    shuffleDeck();
    dealCards();
    setupDiscardPile();
    currentPlayer = 0;
    direction = 1;
    gameOver = false;
    winnerIndex = -1;
    termHeight = termh();
    termWidth = termw();
    lastHandSizes.assign(players.size(), -1);
}

void UNOGame::createDeck() {
    drawPile.clear();
    CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
    for (auto color : colors) {
        drawPile.push_back(Card(color, NUMBER, 0));
        for (int num = 1; num <= 9; ++num) {
            drawPile.push_back(Card(color, NUMBER, num));
            drawPile.push_back(Card(color, NUMBER, num));
        }
        for (int i = 0; i < 2; ++i) {
            drawPile.push_back(Card(color, SKIP));
            drawPile.push_back(Card(color, REVERSE));
            drawPile.push_back(Card(color, DRAW_TWO));
        }
    }
    for (int i = 0; i < 4; ++i) {
        drawPile.push_back(Card(COLOR_NONE, WILD));
        drawPile.push_back(Card(COLOR_NONE, WILD_DRAW_FOUR));
    }
}

void UNOGame::shuffleDeck() {
    static mt19937 rng(random_device{}());
    shuffle(drawPile.begin(), drawPile.end(), rng);
}

void UNOGame::dealCards() {
    for (int i = 0; i < 7; ++i)
        for (auto& player : players) {
            if (drawPile.empty()) reshuffleDiscard();
            player.addCard(drawPile.back());
            drawPile.pop_back();
        }
}

void UNOGame::setupDiscardPile() {
    do {
        if (drawPile.empty()) reshuffleDiscard();
        discardPile.push_back(drawPile.back());
        drawPile.pop_back();
    } while (discardPile.back().type == WILD_DRAW_FOUR);
}

void UNOGame::reshuffleDiscard() {
    if (discardPile.size() <= 1) return;
    Card top = discardPile.back();
    discardPile.pop_back();
    for (const auto& card : discardPile)
        drawPile.push_back(card);
    discardPile.clear();
    discardPile.push_back(top);
    shuffleDeck();
}

Card UNOGame::drawCard() {
    if (drawPile.empty()) reshuffleDiscard();
    Card c = drawPile.back();
    drawPile.pop_back();
    return c;
}

bool UNOGame::isLegalPlay(const Card& played, const Card& top) const {
    if (played.type == WILD || played.type == WILD_DRAW_FOUR) return true;
    if (played.color == top.color) return true;
    if (played.type == top.type && played.type != NUMBER) return true;
    if (played.type == NUMBER && top.type == NUMBER && played.number == top.number) return true;
    return false;
}

void UNOGame::applyCardEffect(const Card& card) {
    int next = getNextPlayer();
    
    if (card.type == DRAW_TWO) {
        for (int i = 0; i < 2; ++i)
            players[next].addCard(drawCard());
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == WILD_DRAW_FOUR) {
        for (int i = 0; i < 4; ++i)
            players[next].addCard(drawCard());
        CardColor newColor = chooseWildColor();
        discardPile.back().color = newColor;
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == SKIP) {
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == REVERSE) {
        if (players.size() == 2) {
            currentPlayer = getNextPlayerAfterSkip(next);
        } else {
            direction *= -1;
            currentPlayer = getNextPlayer();
        }
    }
    else {
        currentPlayer = next;
    }
    
    if (currentPlayer >= (int)players.size()) currentPlayer = 0;
    if (currentPlayer < 0) currentPlayer = players.size() - 1;
}

int UNOGame::getNextPlayer() const {
    int next = currentPlayer + direction;
    if (next < 0) next = players.size() - 1;
    if (next >= (int)players.size()) next = 0;
    return next;
}

int UNOGame::getNextPlayerAfterSkip(int skipped) {
    int next = skipped + direction;
    if (next < 0) next = players.size() - 1;
    if (next >= (int)players.size()) next = 0;
    return next;
}

CardColor UNOGame::chooseWildColor() {
    if (players[currentPlayer].isAI) {
        int count[4] = {0};
        for (const auto& card : players[currentPlayer].hand) {
            if (card.color == COLOR_RED) count[0]++;
            else if (card.color == COLOR_GREEN) count[1]++;
            else if (card.color == COLOR_BLUE) count[2]++;
            else if (card.color == COLOR_YELLOW) count[3]++;
        }
        int maxIdx = 0;
        for (int i = 1; i < 4; ++i)
            if (count[i] > count[maxIdx]) maxIdx = i;
        CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
        return colors[maxIdx];
    } else {
        drawMessage("请选择新颜色: 0-红, 1-绿, 2-蓝, 3-黄 (按数字键)", YELLOW);
        int choice;
        while (true) {
            char ch = my_getch();
            if (ch >= '0' && ch <= '3') { choice = ch - '0'; break; }
        }
        CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
        clearMessageArea();
        return colors[choice];
    }
}

bool UNOGame::playTurn() {
    Player& player = players[currentPlayer];
    Card topCard = discardPile.back();
    updateUI();
    
    if (player.isAI) {
        drawMessage(player.name + " (AI) 正在思考...", CYAN);
        this_thread::sleep_for(chrono::milliseconds(800));
        
        for (size_t i = 0; i < player.hand.size(); ++i) {
            if (isLegalPlay(player.hand[i], topCard)) {
                Card played = player.hand[i];
                player.removeCard(i);
                discardPile.push_back(played);
                drawMessage(player.name + " 出了: " + played.toString(), played.getColorCode());
                this_thread::sleep_for(chrono::milliseconds(500));
                if (player.hand.empty()) {
                    gameOver = true;
                    winnerIndex = currentPlayer;
                    return true;
                }
                applyCardEffect(played);
                return true;
            }
        }
        
        drawMessage(player.name + " 无牌可出，摸一张牌。", MAGENTA);
        Card newCard = drawCard();
        player.addCard(newCard);
        drawMessage(string("摸到: ") + newCard.toString(), newCard.getColorCode());
        this_thread::sleep_for(chrono::milliseconds(500));
        
        if (isLegalPlay(newCard, topCard)) {
            drawMessage("新摸的牌可以出，自动出牌！", GREEN);
            player.removeCard(player.hand.size() - 1);
            discardPile.push_back(newCard);
            if (player.hand.empty()) {
                gameOver = true;
                winnerIndex = currentPlayer;
                return true;
            }
            applyCardEffect(newCard);
        } else {
            currentPlayer = getNextPlayer();
        }
        return true;
    } else {
        handleHumanTurn();
        return true;
    }
}

int UNOGame::findFirstLegalCardIndex() {
    Card topCard = discardPile.back();
    for (size_t i = 0; i < players[0].hand.size(); ++i)
        if (isLegalPlay(players[0].hand[i], topCard))
            return i;
    return -1;
}

void UNOGame::moveSelectionLeft() {
    if (players[0].hand.empty()) return;
    selectedCardIndex = (selectedCardIndex - 1 + (int)players[0].hand.size()) % players[0].hand.size();
    updateUI();
}

void UNOGame::moveSelectionRight() {
    if (players[0].hand.empty()) return;
    selectedCardIndex = (selectedCardIndex + 1) % (int)players[0].hand.size();
    updateUI();
}

void UNOGame::handleHumanTurn() {
    Card topCard = discardPile.back();
    int firstLegal = findFirstLegalCardIndex();
    if (firstLegal != -1)
        selectedCardIndex = firstLegal;
    else
        selectedCardIndex = 0;
    
    waitingForHumanInput = true;
    updateUI();
    drawMessage("方向键移动选中，Enter出牌，空格摸牌", CYAN);
    
    while (waitingForHumanInput) {
        int key = my_getch();
#ifdef _WIN32
        if (key == 224 || key == 0) {
            key = my_getch();
            if (key == 75) moveSelectionLeft();
            else if (key == 77) moveSelectionRight();
        }
#else
        if (key == 27) {
            if (my_getch() == 91) {
                key = my_getch();
                if (key == 68) moveSelectionLeft();
                else if (key == 67) moveSelectionRight();
            }
        }
#endif
        else if (key == 13) {
            if (firstLegal != -1 && isLegalPlay(players[0].hand[selectedCardIndex], topCard)) {
                Card played = players[0].hand[selectedCardIndex];
                players[0].removeCard(selectedCardIndex);
                discardPile.push_back(played);
                drawMessage(string("你出了: ") + played.toString(), played.getColorCode());
                if (players[0].hand.empty()) {
                    gameOver = true;
                    winnerIndex = currentPlayer;
                    waitingForHumanInput = false;
                    return;
                }
                applyCardEffect(played);
                waitingForHumanInput = false;
                return;
            } else {
                drawMessage("选中的牌不能出！请选择可出的牌（有下划线的牌）。", RED);
                this_thread::sleep_for(chrono::milliseconds(800));
                updateUI();
                drawMessage("方向键移动选中，Enter出牌，空格摸牌", CYAN);
            }
        }
        else if (key == 32) {
            Card newCard = drawCard();
            players[0].addCard(newCard);
            drawMessage(string("你摸到了一张: ") + newCard.toString(), newCard.getColorCode());
            this_thread::sleep_for(chrono::milliseconds(800));
            
            if (isLegalPlay(newCard, topCard)) {
                drawMessage("是否出这张牌? (Y/N)", CYAN);
                int confirm = my_getch();
                if (confirm == 'y' || confirm == 'Y') {
                    players[0].removeCard(players[0].hand.size() - 1);
                    discardPile.push_back(newCard);
                    drawMessage("你出了刚摸的牌！", GREEN);
                    if (players[0].hand.empty()) {
                        gameOver = true;
                        winnerIndex = currentPlayer;
                        waitingForHumanInput = false;
                        return;
                    }
                    applyCardEffect(newCard);
                    waitingForHumanInput = false;
                    return;
                } else {
                    drawMessage("保留新牌，轮到下一家。", DEFAULT);
                    this_thread::sleep_for(chrono::milliseconds(800));
                    currentPlayer = getNextPlayer();
                    waitingForHumanInput = false;
                    return;
                }
            } else {
                drawMessage("这张牌不能出，轮到下一家。", RED);
                this_thread::sleep_for(chrono::milliseconds(800));
                currentPlayer = getNextPlayer();
                waitingForHumanInput = false;
                return;
            }
        }
    }
}

void UNOGame::drawBorder() {
    printf("\033[2J\033[H");
    clrtxt("+", CYAN);
    for (int i = 1; i < termWidth-1; ++i) clrtxt("-", CYAN);
    clrtxt("+", CYAN);
    for (int r = 1; r < termHeight-1; ++r) {
        mvc(0, r); clrtxt("|", CYAN);
        mvc(termWidth-1, r); clrtxt("|", CYAN);
    }
    mvc(0, termHeight-1); clrtxt("+", CYAN);
    for (int i = 1; i < termWidth-1; ++i) clrtxt("-", CYAN);
    clrtxt("+", CYAN);
    mvc(termWidth/2 - 4, 0);
    clrtxt(" UNO ", WHITE, BG_RED, TS_BOLD);
}

void UNOGame::drawCenterPileStatic() {
    int centerX = termWidth / 2 - 7;
    int centerY = termHeight / 2 - 2;
    mvc(centerX, centerY-1); clrtxt(" 弃牌堆 ", CYAN);
    mvc(centerX, centerY+4); clrtxt("摸牌堆", CYAN);
}

void UNOGame::drawPlayerAreaStatic() {
    int n = players.size();
    int topY = 2, bottomY = termHeight - 4, leftX = 2, rightX = termWidth - 12;
    for (int i = 0; i < n; ++i) {
        int x, y;
        if (i == 0) { y = bottomY; x = leftX; }
        else if (i == 1) { y = topY + 4; x = leftX; }
        else if (i == 2) { y = topY + 4; x = rightX; }
        else { y = topY; x = termWidth/2 - 8; }
        mvc(x, y-2);
        if (players[i].isAI) clrtxt(" [AI]", MAGENTA);
    }
}

void UNOGame::drawFullUI() {
    termHeight = termh();
    termWidth = termw();
    drawBorder();
    drawCenterPileStatic();
    drawPlayerAreaStatic();
    updateUI();
}

void UNOGame::drawCardBack(int x, int y) {
    mvc(x, y);     clrtxt("     ", DEFAULT);
    mvc(x, y+1);   clrtxt("UNO ", DEFAULT, BG_BLUE, TS_BOLD);
    mvc(x, y+2);   clrtxt("     ", DEFAULT);
}

void UNOGame::drawCardFace(const Card& card, int x, int y, bool selected, bool legal) {
    string sym = card.toString();
    string content = centerString(sym);
    int fg = card.getColorCode();
    int style = legal ? TS_UNDERLINE : TS_NONE;
    
    mvc(x, y);   clrtxt("     ", DEFAULT);
    mvc(x, y+1); clrtxt(content.c_str(), fg, BG_DEFAULT, style);
    mvc(x, y+2); clrtxt("     ", DEFAULT);
    
    if (selected) {
        mvc(x, y+3); clrtxt("───", CYAN);
    } else {
        mvc(x, y+3); clrtxt("   ", DEFAULT);
    }
}

void UNOGame::updateUI() {
    int n = players.size();
    int topY = 2;
    int bottomY = termHeight - 4;
    int handStartY = bottomY - 1;
    int leftX = 2, rightX = termWidth - 12;
    Card topCard = discardPile.back();
    
    for (int i = 0; i < n; ++i) {
        int x, y;
        if (i == 0) {
            y = handStartY;
            x = leftX;
        } else if (i == 1) {
            y = topY + 4;
            x = leftX;
        } else if (i == 2) {
            y = topY + 4;
            x = rightX;
        } else {
            y = topY;
            x = termWidth/2 - 8;
        }
        
        string playerInfo = players[i].name + " (" + to_string(players[i].getHandSize()) + "张)";
        mvc(x, y-2); clrtxt("                                      ", DEFAULT);
        mvc(x, y-2); clrtxt(playerInfo.c_str(), (i == currentPlayer) ? GREEN : DEFAULT);
        if (players[i].isAI) clrtxt(" [AI]", MAGENTA);
        
        if (i == 0) {
            for (int row = 0; row < 4; ++row) {
                mvc(leftX, y + row);
                clrtxt("                                                                                ", DEFAULT);
            }
            int startX = leftX;
            for (size_t j = 0; j < players[0].hand.size(); ++j) {
                int cardX = startX + j * 6;
                if (cardX + 5 > termWidth) break;
                bool isSelected = (j == selectedCardIndex);
                bool isLegal = isLegalPlay(players[0].hand[j], topCard);
                drawCardFace(players[0].hand[j], cardX, y, isSelected, isLegal);
                mvc(cardX+1, y+3);
                clrtxt(to_string(j), DEFAULT);
            }
        } else {
            int startX = x;
            for (int k = 0; k < 10; ++k) {
                mvc(startX + k * 6, y);   clrtxt("     ", DEFAULT);
                mvc(startX + k * 6, y+1); clrtxt("     ", DEFAULT);
                mvc(startX + k * 6, y+2); clrtxt("     ", DEFAULT);
            }
            for (int k = 0; k < min(8, players[i].getHandSize()); ++k)
                drawCardBack(startX + k * 6, y);
            if (players[i].getHandSize() > 8) {
                mvc(startX + 8*6, y+1); clrtxt("...", DEFAULT);
            }
        }
    }
    
    if (!discardPile.empty() && (firstDraw || !(lastTopCard == discardPile.back()))) {
        int centerX = termWidth / 2 - 7, centerY = termHeight / 2 - 2;
        for (int row = 0; row < 3; ++row) { mvc(centerX, centerY+row); clrtxt("          ", DEFAULT); }
        drawCardFace(discardPile.back(), centerX, centerY, false, false);
        lastTopCard = discardPile.back();
    }
    
    if (firstDraw || lastDrawPileSize != (int)drawPile.size()) {
        int centerX = termWidth / 2 - 7, centerY = termHeight / 2 - 2;
        mvc(centerX, centerY+5); clrtxt("          ", DEFAULT);
        mvc(centerX, centerY+5);
        if (!drawPile.empty()) drawCardBack(centerX, centerY+5);
        else clrtxt("(空)", DEFAULT);
        lastDrawPileSize = drawPile.size();
    }
    
    int dirX = termWidth-30, dirY = termHeight/2;
    mvc(dirX, dirY); clrtxt("出牌方向: ", CYAN);
    string dirText = (direction == 1) ? "顺时针 →" : "逆时针 ←";
    clrtxt(dirText.c_str(), YELLOW);
    
    firstDraw = false;
}

void UNOGame::drawMessage(const string& msg, int color) {
    int msgY = termHeight;
    mvc(2, msgY); clrtxt("                                                                                ", DEFAULT, BG_DEFAULT);
    mvc(2, msgY); clrtxt(msg.c_str(), color);
}

void UNOGame::clearMessageArea() {
    drawMessage("", DEFAULT);
}

void UNOGame::run() {
    drawFullUI();
    while (!gameOver) {
        playTurn();
        if (gameOver) break;
        if (players[currentPlayer].isAI)
            this_thread::sleep_for(chrono::milliseconds(600));
    }
    string victoryMsg = string("游戏结束！胜利者是: ") + players[winnerIndex].name;
    drawMessage(victoryMsg, GREEN);
    mvc(2, termHeight-1); clrtxt("按回车键退出...", DEFAULT);
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#else
    init_input();
    atexit(restore_input);
#endif
    setup();
    hc();
    UNOGame game;
    game.run();
    sc();
    return 0;
}