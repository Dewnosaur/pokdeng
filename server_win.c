#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")

#define DECK_SIZE 52

typedef struct
{
    char *rank;
    char *suit;
    int value;
} Card;

Card deck[DECK_SIZE];
int deck_index = 0;

char *ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0};
char *suits[] = {"Spades", "Hearts", "Diamonds", "Clubs"}; // changed from symbols to words

void shuffle_deck()
{
    int i = 0;
    for (int s = 0; s < 4; s++)
    {
        for (int r = 0; r < 13; r++)
        {
            deck[i].rank = ranks[r];
            deck[i].suit = suits[s];
            deck[i].value = values[r];
            i++;
        }
    }
    srand((unsigned int)time(NULL));
    for (int i = 0; i < DECK_SIZE; i++)
    {
        int j = rand() % DECK_SIZE;
        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
    deck_index = 0;
}

Card draw_card()
{
    return deck[deck_index++];
}

int card_value(Card c)
{
    for (int i = 0; i < 13; i++)
    {
        if (strcmp(c.rank, ranks[i]) == 0)
            return values[i];
    }
    return 0;
}

int sum_points(Card cards[], int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++)
    {
        sum += card_value(cards[i]);
    }
    return sum % 10;
}

int is_same_suit(Card a, Card b)
{
    return strcmp(a.suit, b.suit) == 0;
}

int is_triple(Card cards[])
{
    return strcmp(cards[0].rank, cards[1].rank) == 0 &&
           strcmp(cards[1].rank, cards[2].rank) == 0;
}

int is_sequence(Card cards[])
{
    int vals[3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 13; j++)
        {
            if (strcmp(cards[i].rank, ranks[j]) == 0)
            {
                vals[i] = j;
                break;
            }
        }
    }
    // Sort vals ascending
    for (int i = 0; i < 2; i++)
    {
        for (int j = i + 1; j < 3; j++)
        {
            if (vals[i] > vals[j])
            {
                int t = vals[i];
                vals[i] = vals[j];
                vals[j] = t;
            }
        }
    }
    return vals[1] == vals[0] + 1 && vals[2] == vals[1] + 1;
}

int is_flush(Card cards[])
{
    return strcmp(cards[0].suit, cards[1].suit) == 0 &&
           strcmp(cards[1].suit, cards[2].suit) == 0;
}

// Modified two-card evaluation: if both cards are identical and "8" or "9", count as pok.
int evaluate_hand(Card cards[], int count)
{
    if (count == 2)
    {
        if (strcmp(cards[0].rank, cards[1].rank) == 0 &&
            (strcmp(cards[0].rank, "8") == 0 || strcmp(cards[0].rank, "9") == 0))
        {
            return 100 + atoi(cards[0].rank);
        }
        int point = sum_points(cards, 2);
        return point;
    }
    else
    {
        if (is_triple(cards))
            return 90;
        if (is_sequence(cards) && is_flush(cards))
            return 80;
        if (is_sequence(cards))
            return 70;
        return sum_points(cards, 3);
    }
}

// Sends card info to the given socket.
void send_card(SOCKET sock, const char *prefix, Card c)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s:%s of %s\n", prefix, c.rank, c.suit);
    send(sock, buffer, strlen(buffer), 0);
}

int recv_draw_decision(SOCKET client)
{
    char buffer[128];
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0)
            return 0; // connection closed or error
        buffer[len] = '\0';
        if (strncmp(buffer, "DRAW3", 5) == 0)
        {
            return 1;
        }
        else if (strncmp(buffer, "NO", 2) == 0)
        {
            return 0;
        }
        else
        {
            printf("[Client]: Invalid command received: %s\n", buffer);
        }
    }
}

// New structure and function for detailed hand evaluation
typedef struct
{
    int rank_value;
    char description[64];
    int high_card; // tie-breaker for straight, straightflush or triple; -1 if N/A
} HandResult;

HandResult get_hand_result(Card cards[], int count)
{
    HandResult res;
    if (count == 2)
    {
        if (strcmp(cards[0].rank, cards[1].rank) == 0 &&
            (strcmp(cards[0].rank, "8") == 0 || strcmp(cards[0].rank, "9") == 0))
        {
            res.rank_value = 100 + atoi(cards[0].rank);
            sprintf(res.description, "Pok %s", cards[0].rank);
            res.high_card = -1;
        }
        else
        {
            int pts = sum_points(cards, 2);
            res.rank_value = pts;
            sprintf(res.description, "score %d", pts);
            res.high_card = -1;
        }
    }
    else
    { // count == 3
        if (is_triple(cards))
        {
            res.rank_value = 90;
            sprintf(res.description, "Three of a kind");
            for (int j = 0; j < 13; j++)
            {
                if (strcmp(cards[0].rank, ranks[j]) == 0)
                {
                    res.high_card = j;
                    break;
                }
            }
        }
        else if (is_sequence(cards) && is_flush(cards))
        {
            res.rank_value = 80;
            int vals[3];
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 13; j++)
                {
                    if (strcmp(cards[i].rank, ranks[j]) == 0)
                    {
                        vals[i] = j;
                        break;
                    }
                }
            }
            // Sort vals ascending
            for (int i = 0; i < 2; i++)
            {
                for (int j = i + 1; j < 3; j++)
                {
                    if (vals[i] > vals[j])
                    {
                        int t = vals[i];
                        vals[i] = vals[j];
                        vals[j] = t;
                    }
                }
            }
            int high = vals[2];
            sprintf(res.description, "straightflush (high %s)", ranks[high]);
            res.high_card = high;
        }
        else if (is_sequence(cards))
        {
            res.rank_value = 70;
            int vals[3];
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 13; j++)
                {
                    if (strcmp(cards[i].rank, ranks[j]) == 0)
                    {
                        vals[i] = j;
                        break;
                    }
                }
            }
            // Sort vals ascending
            for (int i = 0; i < 2; i++)
            {
                for (int j = i + 1; j < 3; j++)
                {
                    if (vals[i] > vals[j])
                    {
                        int t = vals[i];
                        vals[i] = vals[j];
                        vals[j] = t;
                    }
                }
            }
            int high = vals[2];
            sprintf(res.description, "straight (high %s)", ranks[high]);
            res.high_card = high;
        }
        else
        {
            int pts = sum_points(cards, 3);
            res.rank_value = pts;
            sprintf(res.description, "score %d", pts);
            res.high_card = -1;
        }
    }
    return res;
}

int main()
{
    WSADATA wsaData;
    SOCKET server, client;
    struct sockaddr_in server_addr, client_addr;
    int addr_size = sizeof(client_addr);

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    server = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (SOCKADDR *)&server_addr, sizeof(server_addr));
    listen(server, 1);

    printf("[Server]: Waiting for client to connect...\n");
    client = accept(server, (SOCKADDR *)&client_addr, &addr_size);
    printf("[Server]: Client connected.\n");

    shuffle_deck();

    Card client_cards[3], dealer_cards[3];
    // For testing, force client to have two "8" cards.
    client_cards[0] = draw_card();
    client_cards[1] = draw_card();
    dealer_cards[0] = draw_card();
    dealer_cards[1] = draw_card();

    // Send 2 cards to client first
    send_card(client, "CARD1", client_cards[0]);
    send_card(client, "CARD2", client_cards[1]);

    HandResult initial = get_hand_result(client_cards, 2);
    int client_draw3;
    if (initial.rank_value >= 108)
    { // pok hand detected.
        client_draw3 = 0;
        // Send the pok message (e.g. "pok 8" or "pok 9")
        char poke_msg[32];
        sprintf(poke_msg, "%s\n", initial.description);
        send(client, poke_msg, strlen(poke_msg), 0);
    }
    else
    {
        // Tell client to decide draw or not
        send(client, "WAIT_FOR_DRAW\n", strlen("WAIT_FOR_DRAW\n"), 0);

        // Wait for client's decision
        int client_draw3 = recv_draw_decision(client);
        int client_score;
        if (client_draw3)
        {
            client_cards[2] = draw_card();
            send_card(client, "CARD3", client_cards[2]);
            client_score = evaluate_hand(client_cards, 3);
            send(client, "WAITING FOR DEALER\n", strlen("WAITING FOR DEALER\n"), 0);
        }
        else
        {
            client_score = evaluate_hand(client_cards, 2);
            send(client, "WAITING FOR DEALER\n", strlen("WAITING FOR DEALER\n"), 0);
        }
    }

    // Dealer decides to draw or not
    printf("\n[Dealer]: Your hand: %s of %s, %s of %s\n",
           dealer_cards[0].rank, dealer_cards[0].suit,
           dealer_cards[1].rank, dealer_cards[1].suit);

    char dealer_choice[4];

    HandResult dealerInitial = get_hand_result(dealer_cards, 2);
    int dealer_draw3;
    if (dealerInitial.rank_value >= 108) { // dealer has pok 8 or pok 9
        dealer_draw3 = 0;
        printf("[Dealer]: %s\n", dealerInitial.description);
    } else {
        printf("[Dealer]: Do you want to draw a third card? (y/n): ");
        char dealer_choice[4];
        fgets(dealer_choice, sizeof(dealer_choice), stdin);
        dealer_draw3 = (dealer_choice[0]=='y' || dealer_choice[0]=='Y') ? 1 : 0;
    }

    int dealer_score;
    if (dealer_draw3) {
        dealer_cards[2] = draw_card();
        dealer_score = evaluate_hand(dealer_cards, 3);
        printf("[Dealer]: Drew %s of %s\n", dealer_cards[2].rank, dealer_cards[2].suit);
    } else {
        dealer_score = evaluate_hand(dealer_cards, 2);
    }
    printf("[Dealer]: Your final score: %d\n", dealer_score % 100);

    // Compare results using detailed hand evaluation
    HandResult clientResult, dealerResult;
    if (client_draw3)
    {
        clientResult = get_hand_result(client_cards, 3);
    }
    else
    {
        clientResult = get_hand_result(client_cards, 2);
    }
    if (dealer_choice[0] == 'y' || dealer_choice[0] == 'Y')
    {
        dealerResult = get_hand_result(dealer_cards, 3);
    }
    else
    {
        dealerResult = get_hand_result(dealer_cards, 2);
    }

    char result[128];
    if (clientResult.rank_value > dealerResult.rank_value)
    {
        sprintf(result, "RESULT: You win! (%s vs %s)\n", clientResult.description, dealerResult.description);
    }
    else if (clientResult.rank_value < dealerResult.rank_value)
    {
        sprintf(result, "RESULT: Dealer wins! (%s vs %s)\n", dealerResult.description, clientResult.description);
    }
    else
    {
        if (clientResult.high_card != -1 && dealerResult.high_card != -1)
        {
            if (clientResult.high_card > dealerResult.high_card)
            {
                sprintf(result, "RESULT: You win! (%s vs %s)\n", clientResult.description, dealerResult.description);
            }
            else if (clientResult.high_card < dealerResult.high_card)
            {
                sprintf(result, "RESULT: Dealer wins! (%s vs %s)\n", dealerResult.description, clientResult.description);
            }
            else
            {
                sprintf(result, "RESULT: Draw (%s)\n", clientResult.description);
            }
        }
        else
        {
            sprintf(result, "RESULT: Draw (%s)\n", clientResult.description);
        }
    }
    send(client, result, strlen(result), 0);

    closesocket(client);
    closesocket(server);
    WSACleanup();
    return 0;
}