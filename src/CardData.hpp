#pragma once

enum class CardType {
    HIKARI,
    TANE,
    TANZAKU,
    KASU
};

enum class RibbonType {
    NONE,
    RED_POETRY,
    BLUE,
    RED_PLAIN
};

struct Card {
    int id;        // Month-major: (month - 1) * 4 + cardIndex.
    int month;     // 1 through 12.
    int cardIndex; // Atlas row, 0 through 3.
    CardType type;
    RibbonType ribbon;
    bool isSakeCup;
    const char* name; // Points to a static string literal.
};
