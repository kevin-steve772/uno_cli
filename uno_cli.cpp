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
#include <conio.h>
#include <sstream>
#include <limits>

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
    int lastSelectedIndex;   // 新增：上一次选中的卡片索引，用于轻量刷新
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
    void updateSelection(int oldIdx, int newIdx);  // 新增：仅刷新选中变化
    void drawBorder();
    void drawMessage(const string& msg, int color = YELLOW);
    void clearMessageArea();
    void handleHumanTurn();
    void moveSelectionLeft();
    void moveSelectionRight();
    string centerString(const string& sym) const {
        if (sym.size() == 1) return string("  ") + sym + string("  ");
        else return string(" ") + sym + string("  ");
    }
};

UNOGame::UNOGame() : lastCurrentPlayer(-1), lastDrawPileSize(-1), firstDraw(true), selectedCardIndex(0), lastSelectedIndex(-1), waitingForHumanInput(false) {
    srand(time(nullptr));
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
    createDeck(); shuffleDeck(); dealCards(); setupDiscardPile();
    currentPlayer = 0; direction = 1; gameOver = false; winnerIndex = -1;
    termHeight = termh(); termWidth = termw();
    lastHandSizes.resize(players.size(), -1);
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
            player.addCard(drawPile.back()); drawPile.pop_back();
        }
}

void UNOGame::setupDiscardPile() {
    do {
        if (drawPile.empty()) reshuffleDiscard();
        discardPile.push_back(drawPile.back()); drawPile.pop_back();
    } while (discardPile.back().type == WILD_DRAW_FOUR);
}

void UNOGame::reshuffleDiscard() {
    if (discardPile.size() <= 1) return;
    Card top = discardPile.back(); discardPile.pop_back();
    for (const auto& card : discardPile) drawPile.push_back(card);
    discardPile.clear(); discardPile.push_back(top);
    shuffleDeck();
}

Card UNOGame::drawCard() {
    if (drawPile.empty()) reshuffleDiscard();
    Card c = drawPile.back(); drawPile.pop_back();
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
    if (card.type == DRAW_TWO) {
        int next = getNextPlayer();
        for (int i = 0; i < 2; ++i) players[next].addCard(drawCard());
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == WILD_DRAW_FOUR) {
        int next = getNextPlayer();
        for (int i = 0; i < 4; ++i) players[next].addCard(drawCard());
        CardColor newColor = chooseWildColor();
        discardPile.back().color = newColor;
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == SKIP) {
        int next = getNextPlayer();
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == REVERSE) {
        direction *= -1;
        currentPlayer = getNextPlayer();
    }
    else {
        currentPlayer = getNextPlayer();
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
        for (int i = 1; i < 4; ++i) if (count[i] > count[maxIdx]) maxIdx = i;
        CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
        return colors[maxIdx];
    } else {
        drawMessage(string("请选择新颜色: 0-红, 1-绿, 2-蓝, 3-黄 (按数字键)"), YELLOW);
        int choice;
        while (true) {
            char ch = _getch();
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
        drawMessage(player.name + string(" (AI) 正在思考..."), CYAN);
        this_thread::sleep_for(chrono::milliseconds(800));
        for (size_t i = 0; i < player.hand.size(); ++i) {
            if (isLegalPlay(player.hand[i], topCard)) {
                Card played = player.hand[i];
                player.removeCard(i);
                discardPile.push_back(played);
                drawMessage(player.name + string(" 出了: ") + played.toString(), played.getColorCode());
                this_thread::sleep_for(chrono::milliseconds(500));
                if (player.hand.empty()) { gameOver = true; winnerIndex = currentPlayer; return true; }
                applyCardEffect(played);
                return true;
            }
        }
        drawMessage(player.name + string(" 无牌可出，摸一张牌。"), MAGENTA);
        Card newCard = drawCard();
        player.addCard(newCard);
        drawMessage(string("摸到: ") + newCard.toString(), newCard.getColorCode());
        this_thread::sleep_for(chrono::milliseconds(500));
        if (isLegalPlay(newCard, topCard)) {
            drawMessage(string("新摸的牌可以出，自动出牌！"), GREEN);
            player.removeCard(player.hand.size() - 1);
            discardPile.push_back(newCard);
            if (player.hand.empty()) { gameOver = true; winnerIndex = currentPlayer; return true; }
            applyCardEffect(newCard);
        } else currentPlayer = getNextPlayer();
        return true;
    } else {
        handleHumanTurn();
        return true;
    }
}

void UNOGame::moveSelectionLeft() {
    if (players[0].hand.empty()) return;
    int oldIdx = selectedCardIndex;
    selectedCardIndex = (selectedCardIndex - 1 + (int)players[0].hand.size()) % players[0].hand.size();
    if (oldIdx != selectedCardIndex)
        updateSelection(oldIdx, selectedCardIndex);
}

void UNOGame::moveSelectionRight() {
    if (players[0].hand.empty()) return;
    int oldIdx = selectedCardIndex;
    selectedCardIndex = (selectedCardIndex + 1) % (int)players[0].hand.size();
    if (oldIdx != selectedCardIndex)
        updateSelection(oldIdx, selectedCardIndex);
}

void UNOGame::handleHumanTurn() {
    Card topCard = discardPile.back();
    selectedCardIndex = 0;
    lastSelectedIndex = -1;   // 确保第一次刷新正确
    waitingForHumanInput = true;
    updateUI();
    drawMessage(string("方向键移动选中，Enter出牌，空格摸牌"), CYAN);
    while (waitingForHumanInput) {
        int key = _getch();
        if (key == 224 || key == 0) {
            key = _getch();
            if (key == 75) moveSelectionLeft();
            else if (key == 77) moveSelectionRight();
        } else if (key == 13) {
            if (isLegalPlay(players[0].hand[selectedCardIndex], topCard)) {
                Card played = players[0].hand[selectedCardIndex];
                players[0].removeCard(selectedCardIndex);
                discardPile.push_back(played);
                drawMessage(string("你出了: ") + played.toString(), played.getColorCode());
                if (players[0].hand.empty()) { gameOver = true; winnerIndex = currentPlayer; waitingForHumanInput = false; return; }
                applyCardEffect(played);
                waitingForHumanInput = false; return;
            } else {
                drawMessage(string("选中的牌不能出！请选择可出的牌（有下划线的牌）。"), RED);
                this_thread::sleep_for(chrono::milliseconds(800));
                updateUI();
                drawMessage(string("方向键移动选中，Enter出牌，空格摸牌"), CYAN);
            }
        } else if (key == 32) {
            Card newCard = drawCard();
            players[0].addCard(newCard);
            drawMessage(string("你摸到了一张: ") + newCard.toString(), newCard.getColorCode());
            this_thread::sleep_for(chrono::milliseconds(800));
            if (isLegalPlay(newCard, topCard)) {
                drawMessage(string("是否出这张牌? (Y/N)"), CYAN);
                int confirm = _getch();
                if (confirm == 'y' || confirm == 'Y') {
                    players[0].removeCard(players[0].hand.size() - 1);
                    discardPile.push_back(newCard);
                    drawMessage(string("你出了刚摸的牌！"), GREEN);
                    if (players[0].hand.empty()) { gameOver = true; winnerIndex = currentPlayer; waitingForHumanInput = false; return; }
                    applyCardEffect(newCard);
                    waitingForHumanInput = false; return;
                } else {
                    drawMessage(string("保留新牌，轮到下一家。"), DEFAULT);
                    this_thread::sleep_for(chrono::milliseconds(800));
                    currentPlayer = getNextPlayer();
                    waitingForHumanInput = false; return;
                }
            } else {
                drawMessage(string("这张牌不能出，轮到下一家。"), RED);
                this_thread::sleep_for(chrono::milliseconds(800));
                currentPlayer = getNextPlayer();
                waitingForHumanInput = false; return;
            }
        }
    }
}

// ======================= 界面绘制 =======================
void UNOGame::drawBorder() {
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

void UNOGame::drawFullUI() {
    termHeight = termh(); termWidth = termw();
    printf("\033[2J\033[H");
    drawBorder();
    int centerX = termWidth / 2 - 7;
    int centerY = termHeight / 2 - 2;
    mvc(centerX, centerY-1); clrtxt(" 弃牌堆 ", CYAN);
    mvc(centerX, centerY+4); clrtxt("摸牌堆", CYAN);
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
    updateUI();
}

void UNOGame::updateUI() {
    ostringstream oss;
    auto out_mvc = [&](int x, int y) { oss << "\033[" << y << ";" << x << "H"; };
    auto out_clrtxt = [&](const string& text, int fg = DEFAULT, int bg = BG_DEFAULT, int style = TS_NONE) {
        if (style == TS_NONE)
            oss << "\033[" << fg << ";" << bg << "m" << text << "\033[0m";
        else
            oss << "\033[" << style << ";" << fg << ";" << bg << "m" << text << "\033[0m";
    };
    auto out_fixed = [&](const string& text, int width, int fg = DEFAULT, int bg = BG_DEFAULT, int style = TS_NONE) {
        string padded = text;
        if ((int)padded.size() < width) padded.append(width - padded.size(), ' ');
        out_clrtxt(padded, fg, bg, style);
    };
    auto draw_card_face = [&](const Card& card, int x, int y, bool selected, bool legal) {
        string sym = card.toString();
        string content = centerString(sym);
        int fg = card.getColorCode();
        int style = legal ? TS_UNDERLINE : TS_NONE;
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt(content, fg, BG_DEFAULT, style);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        if (selected) {
            out_mvc(x, y+3); out_clrtxt("───", CYAN);
        } else {
            out_mvc(x, y+3); out_clrtxt("   ", DEFAULT);
        }
    };
    auto draw_card_back = [&](int x, int y) {
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt("UNO ", DEFAULT, BG_BLUE, TS_BOLD);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
    };
    auto clear_card_area = [&](int x, int y) {
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+3); out_clrtxt("   ", DEFAULT);
    };

    int n = players.size();
    int topY = 2;
    int bottomY = termHeight - 4;
    int handStartY = bottomY - 1;
    int leftX = 2, rightX = termWidth - 12;
    Card topCard = discardPile.back();

    for (int i = 0; i < n; ++i) {
        int x, y;
        if (i == 0) { y = handStartY; x = leftX; }
        else if (i == 1) { y = topY + 4; x = leftX; }
        else if (i == 2) { y = topY + 4; x = rightX; }
        else { y = topY; x = termWidth/2 - 8; }

        // 玩家信息行
        string playerInfo = players[i].name + " (" + to_string(players[i].getHandSize()) + "张)";
        out_mvc(x, y-2);
        out_fixed(playerInfo, 40, (i == currentPlayer) ? GREEN : DEFAULT);
        if (players[i].isAI) out_clrtxt(" [AI]", MAGENTA);

        if (i == 0) { // 人类玩家手牌
            int maxCards = (termWidth - leftX) / 6;
            int handSize = players[0].hand.size();
            // 清除右侧可能存在的多余卡片
            for (int k = handSize; k < maxCards; ++k) {
                clear_card_area(leftX + k * 6, y);
            }
            // 绘制所有手牌
            for (int j = 0; j < handSize; ++j) {
                int cardX = leftX + j * 6;
                if (cardX + 5 > termWidth) break;
                bool isSelected = (j == selectedCardIndex);
                bool isLegal = isLegalPlay(players[0].hand[j], topCard);
                draw_card_face(players[0].hand[j], cardX, y, isSelected, isLegal);
            }
        } else { // AI 手牌
            int startX = x;
            int maxDisplay = 8;
            int handSize = players[i].getHandSize();
            // 清除右侧多余卡片位置
            for (int k = handSize; k < maxDisplay; ++k) {
                clear_card_area(startX + k * 6, y);
            }
            // 清除省略号位置
            clear_card_area(startX + maxDisplay * 6, y);
            // 绘制前 min(handSize, maxDisplay) 张背面
            int drawCount = min(handSize, maxDisplay);
            for (int k = 0; k < drawCount; ++k) {
                draw_card_back(startX + k * 6, y);
            }
            // 如果手牌超过 maxDisplay，显示省略号
            if (handSize > maxDisplay) {
                out_mvc(startX + maxDisplay * 6, y+1);
                out_clrtxt("...", DEFAULT);
            }
        }
    }

    // 弃牌堆
    if (!discardPile.empty()) {
        int centerX = termWidth / 2 - 7, centerY = termHeight / 2 - 2;
        draw_card_face(discardPile.back(), centerX, centerY, false, false);
    }

    // 摸牌堆
    int centerX = termWidth / 2 - 7, centerY = termHeight / 2 - 2;
    out_mvc(centerX, centerY+5);
    if (!drawPile.empty()) draw_card_back(centerX, centerY+5);
    else out_clrtxt("(空)", DEFAULT);
    lastDrawPileSize = drawPile.size();

    // 方向指示
    int dirX = termWidth-30, dirY = termHeight/2;
    out_mvc(dirX, dirY); out_clrtxt("出牌方向: ", CYAN);
    string dirText = (direction == 1) ? "顺时针 →" : "逆时针 ←";
    out_clrtxt(dirText, YELLOW);

    firstDraw = false;
    printf("%s", oss.str().c_str());
    fflush(stdout);
    
    // 更新上次选中的索引，用于轻量刷新
    lastSelectedIndex = selectedCardIndex;
}

// 轻量级刷新：仅重绘两张卡片（旧选中和新选中）的高亮状态
void UNOGame::updateSelection(int oldIdx, int newIdx) {
    if (oldIdx == newIdx) return;
    if (players[0].hand.empty()) return;
    
    ostringstream oss;
    auto out_mvc = [&](int x, int y) { oss << "\033[" << y << ";" << x << "H"; };
    auto out_clrtxt = [&](const string& text, int fg = DEFAULT, int bg = BG_DEFAULT, int style = TS_NONE) {
        if (style == TS_NONE)
            oss << "\033[" << fg << ";" << bg << "m" << text << "\033[0m";
        else
            oss << "\033[" << style << ";" << fg << ";" << bg << "m" << text << "\033[0m";
    };
    auto draw_card_face = [&](const Card& card, int x, int y, bool selected, bool legal) {
        string sym = card.toString();
        string content = centerString(sym);
        int fg = card.getColorCode();
        int style = legal ? TS_UNDERLINE : TS_NONE;
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt(content, fg, BG_DEFAULT, style);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        if (selected) {
            out_mvc(x, y+3); out_clrtxt("───", CYAN);
        } else {
            out_mvc(x, y+3); out_clrtxt("   ", DEFAULT);
        }
    };
    
    // 计算手牌起始坐标（与 updateUI 保持一致）
    int bottomY = termHeight - 4;
    int handStartY = bottomY - 1;
    int leftX = 2;
    int startX = leftX;
    int cardWidth = 6;
    int y = handStartY;
    
    Card topCard = discardPile.back();
    
    // 重绘旧位置的卡片（取消高亮）
    if (oldIdx >= 0 && oldIdx < (int)players[0].hand.size()) {
        int cardX = startX + oldIdx * cardWidth;
        bool isLegal = isLegalPlay(players[0].hand[oldIdx], topCard);
        draw_card_face(players[0].hand[oldIdx], cardX, y, false, isLegal);
    }
    // 重绘新位置的卡片（高亮）
    if (newIdx >= 0 && newIdx < (int)players[0].hand.size()) {
        int cardX = startX + newIdx * cardWidth;
        bool isLegal = isLegalPlay(players[0].hand[newIdx], topCard);
        draw_card_face(players[0].hand[newIdx], cardX, y, true, isLegal);
    }
    
    printf("%s", oss.str().c_str());
    fflush(stdout);
    
    lastSelectedIndex = newIdx;
}

void UNOGame::drawMessage(const string& msg, int color) {
    int msgY = termHeight;
    mvc(2, msgY); clrtxt("                                                                                ", DEFAULT, BG_DEFAULT);
    mvc(2, msgY); clrtxt(msg.c_str(), color);
}

void UNOGame::clearMessageArea() { drawMessage("", DEFAULT); }

void UNOGame::run() {
    drawFullUI();
    while (!gameOver) {
        playTurn();
        if (gameOver) break;
        if (players[currentPlayer].isAI) this_thread::sleep_for(chrono::milliseconds(600));
    }
    string victoryMsg = string("游戏结束！胜利者是: ") + players[winnerIndex].name;
    drawMessage(victoryMsg, GREEN);
    mvc(2, termHeight-1); clrtxt("按回车键退出...", DEFAULT);
    cin.ignore(); cin.get();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    setup(); hc(); UNOGame game; game.run(); sc();
    return 0;
}