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

using namespace std;

// 自定义卡片颜色枚举（避免与console_ui.h中的Color冲突）
enum CardColor {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_NONE
};

// 卡片类型枚举
enum CardType {
    NUMBER, SKIP, REVERSE, DRAW_TWO, WILD, WILD_DRAW_FOUR
};

// 卡片结构体
struct Card {
    CardColor color;
    CardType type;
    int number; // 仅当type为NUMBER时有效

    Card(CardColor c = COLOR_NONE, CardType t = NUMBER, int n = 0) : color(c), type(t), number(n) {}

    // 获取卡片显示字符串
    string toString() const {
        string colorStr;
        switch (color) {
            case COLOR_RED:    colorStr = "R"; break;
            case COLOR_GREEN:  colorStr = "G"; break;
            case COLOR_BLUE:   colorStr = "B"; break;
            case COLOR_YELLOW: colorStr = "Y"; break;
            default:           colorStr = "W"; break;
        }
        if (type == NUMBER) {
            return colorStr + to_string(number);
        } else if (type == SKIP) {
            return colorStr + "S";
        } else if (type == REVERSE) {
            return colorStr + "R";
        } else if (type == DRAW_TWO) {
            return colorStr + "+2";
        } else if (type == WILD) {
            return "WD";
        } else if (type == WILD_DRAW_FOUR) {
            return "W+4";
        }
        return "???";
    }

    // 获取控制台颜色代码（映射到console_ui.h中的Color枚举值）
    int getColorCode() const {
        switch (color) {
            case COLOR_RED:    return RED;    // 31
            case COLOR_GREEN:  return GREEN;  // 32
            case COLOR_BLUE:   return BLUE;   // 34
            case COLOR_YELLOW: return YELLOW; // 33
            default:           return DEFAULT;
        }
    }
};

// 玩家类
class Player {
public:
    string name;
    bool isAI;
    vector<Card> hand;

    Player(const string& n, bool ai) : name(n), isAI(ai) {}

    void addCard(const Card& card) {
        hand.push_back(card);
    }

    void removeCard(int index) {
        hand.erase(hand.begin() + index);
    }

    // 显示手牌（带索引）
    void showHand() const {
        for (size_t i = 0; i < hand.size(); ++i) {
            const Card& c = hand[i];
            clrtxt("[" + to_string(i) + "] ", DEFAULT);
            clrtxt(c.toString(), c.getColorCode());
            cout << " ";
        }
        cout << endl;
    }

    // 获取手牌数量
    int getHandSize() const {
        return hand.size();
    }
};

// 游戏主类
class UNOGame {
private:
    vector<Player> players;
    vector<Card> drawPile;    // 摸牌堆
    vector<Card> discardPile; // 弃牌堆
    int currentPlayer;        // 当前玩家索引
    int direction;            // 1=顺时针, -1=逆时针
    bool gameOver;
    int winnerIndex;

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
    void drawUI();
};

// 实现部分
UNOGame::UNOGame() {
    srand(time(nullptr));
    setupPlayers();
    initGame();
}

void UNOGame::setupPlayers() {
    string playerName;
    clrtxt("请输入你的名字: ", CYAN);
    cin >> playerName;
    players.push_back(Player(playerName, false));

    int aiCount;
    clrtxt("请输入AI玩家数量 (1~3): ", CYAN);
    cin >> aiCount;
    aiCount = max(1, min(3, aiCount));
    for (int i = 1; i <= aiCount; ++i) {
        players.push_back(Player("AI_" + to_string(i), true));
    }
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
}

void UNOGame::createDeck() {
    drawPile.clear();
    // 数字牌: 每种颜色0~9，0一张，1~9各两张
    CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
    for (auto color : colors) {
        // 数字0
        drawPile.push_back(Card(color, NUMBER, 0));
        // 数字1~9 各两张
        for (int num = 1; num <= 9; ++num) {
            drawPile.push_back(Card(color, NUMBER, num));
            drawPile.push_back(Card(color, NUMBER, num));
        }
        // 功能牌: SKIP, REVERSE, DRAW_TWO 各两张
        for (int i = 0; i < 2; ++i) {
            drawPile.push_back(Card(color, SKIP));
            drawPile.push_back(Card(color, REVERSE));
            drawPile.push_back(Card(color, DRAW_TWO));
        }
    }
    // 万能牌: WILD 和 WILD_DRAW_FOUR 各4张
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
    for (int i = 0; i < 7; ++i) {
        for (auto& player : players) {
            if (drawPile.empty()) reshuffleDiscard();
            player.addCard(drawPile.back());
            drawPile.pop_back();
        }
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
    for (const auto& card : discardPile) {
        drawPile.push_back(card);
    }
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
    // 处理 +2 或 +4 抽牌
    if (card.type == DRAW_TWO) {
        int next = getNextPlayer();
        for (int i = 0; i < 2; ++i) {
            players[next].addCard(drawCard());
        }
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == WILD_DRAW_FOUR) {
        int next = getNextPlayer();
        for (int i = 0; i < 4; ++i) {
            players[next].addCard(drawCard());
        }
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
        // 普通数字牌或普通万能牌
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
        // AI 选择自己手牌中数量最多的颜色
        int count[4] = {0,0,0,0}; // R,G,B,Y
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
        clrtxt("请选择新颜色: 0-红, 1-绿, 2-蓝, 3-黄\n", YELLOW);
        int choice;
        cin >> choice;
        while (choice < 0 || choice > 3) {
            clrtxt("无效选择，请重新输入: ", RED);
            cin >> choice;
        }
        CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
        return colors[choice];
    }
}

bool UNOGame::playTurn() {
    Player& player = players[currentPlayer];
    Card topCard = discardPile.back();

    drawUI();

    if (player.isAI) {
        clrtxt(player.name + " (AI) 正在思考...\n", CYAN);
        for (size_t i = 0; i < player.hand.size(); ++i) {
            if (isLegalPlay(player.hand[i], topCard)) {
                Card played = player.hand[i];
                player.removeCard(i);
                discardPile.push_back(played);

                clrtxt(player.name + " 出了: ", YELLOW);
                clrtxt(played.toString(), played.getColorCode());
                cout << endl;

                if (player.hand.empty()) {
                    gameOver = true;
                    winnerIndex = currentPlayer;
                    return true;
                }

                applyCardEffect(played);
                return true;
            }
        }
        // 无合法牌，摸一张
        clrtxt(player.name + " 无牌可出，摸一张牌。\n", MAGENTA);
        Card newCard = drawCard();
        player.addCard(newCard);
        clrtxt("摸到: ", DEFAULT);
        clrtxt(newCard.toString(), newCard.getColorCode());
        cout << endl;

        if (isLegalPlay(newCard, topCard)) {
            clrtxt("新摸的牌可以出，自动出牌！\n", GREEN);
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
    }
    else {
        // 人类玩家
        clrtxt("\n你的手牌: ", CYAN);
        player.showHand();
        clrtxt("当前牌堆顶: ", CYAN);
        clrtxt(topCard.toString(), topCard.getColorCode());
        cout << endl;

        vector<int> legalIndices;
        for (size_t i = 0; i < player.hand.size(); ++i) {
            if (isLegalPlay(player.hand[i], topCard)) {
                legalIndices.push_back(i);
            }
        }

        if (!legalIndices.empty()) {
            clrtxt("可出的牌索引: ", GREEN);
            for (int idx : legalIndices) cout << idx << " ";
            cout << endl;

            int choice;
            clrtxt("请选择出牌索引 (或输入 -1 摸牌): ", YELLOW);
            cin >> choice;

            if (choice == -1) {
                Card newCard = drawCard();
                player.addCard(newCard);
                clrtxt("你摸到了一张: ", DEFAULT);
                clrtxt(newCard.toString(), newCard.getColorCode());
                cout << endl;

                if (isLegalPlay(newCard, topCard)) {
                    clrtxt("是否出这张牌? (1=是, 0=否): ", CYAN);
                    int confirm;
                    cin >> confirm;
                    if (confirm == 1) {
                        player.removeCard(player.hand.size() - 1);
                        discardPile.push_back(newCard);
                        clrtxt("你出了刚摸的牌！\n", GREEN);
                        if (player.hand.empty()) {
                            gameOver = true;
                            winnerIndex = currentPlayer;
                            return true;
                        }
                        applyCardEffect(newCard);
                    } else {
                        currentPlayer = getNextPlayer();
                    }
                } else {
                    clrtxt("这张牌不能出，轮到下一家。\n", RED);
                    currentPlayer = getNextPlayer();
                }
                return true;
            }

            bool valid = false;
            for (int idx : legalIndices) if (idx == choice) { valid = true; break; }
            if (!valid || choice < 0 || choice >= (int)player.hand.size()) {
                clrtxt("无效出牌，你失去这一回合！\n", RED);
                currentPlayer = getNextPlayer();
                return true;
            }

            Card played = player.hand[choice];
            player.removeCard(choice);
            discardPile.push_back(played);
            clrtxt("你出了: ", YELLOW);
            clrtxt(played.toString(), played.getColorCode());
            cout << endl;

            if (player.hand.empty()) {
                gameOver = true;
                winnerIndex = currentPlayer;
                return true;
            }
            applyCardEffect(played);
        } else {
            clrtxt("没有可出的牌，必须摸一张。\n", RED);
            Card newCard = drawCard();
            player.addCard(newCard);
            clrtxt("摸到: ", DEFAULT);
            clrtxt(newCard.toString(), newCard.getColorCode());
            cout << endl;
            if (isLegalPlay(newCard, topCard)) {
                clrtxt("新牌可以出，自动出牌！\n", GREEN);
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
        }
        return true;
    }
}

void UNOGame::drawUI() {
    printf("\033[2J\033[H");
    clrtxt("========== UNO 游戏 ==========\n", WHITE, BG_BLUE, TS_BOLD);
    for (size_t i = 0; i < players.size(); ++i) {
        if (i == currentPlayer) clrtxt("-> ", GREEN);
        else clrtxt("   ", DEFAULT);
        clrtxt(players[i].name + " (" + to_string(players[i].getHandSize()) + "张)", CYAN);
        if (players[i].isAI) clrtxt(" [AI]", MAGENTA);
        cout << endl;
    }
    clrtxt("\n当前牌堆顶: ", CYAN);
    clrtxt(discardPile.back().toString(), discardPile.back().getColorCode());
    cout << endl;
    clrtxt("摸牌堆剩余: " + to_string(drawPile.size()) + " 张\n", DEFAULT);
    cout << endl;
}

void UNOGame::run() {
    while (!gameOver) {
        playTurn();
        if (gameOver) break;
        if (players[currentPlayer].isAI) {
            this_thread::sleep_for(chrono::milliseconds(800));
        }
    }
    drawUI();
    clrtxt("\n游戏结束！胜利者是: ", YELLOW, BG_BLACK, TS_BOLD);
    clrtxt(players[winnerIndex].name, GREEN, BG_BLACK, TS_BOLD);
    cout << endl;
    clrtxt("按回车键退出...", DEFAULT);
    cin.ignore();
    cin.get();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // 设置控制台输出代码页为UTF-8
#endif
    setup();   // 启用控制台虚拟终端处理（Windows）
    hc();      // 隐藏光标
    UNOGame game;
    game.run();
    sc();      // 恢复光标
    return 0;
}