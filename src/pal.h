#define SHIPNAME_LEN 30
#define SHIPSTATUS_LEN 16

#define FINANCE_ENTRY_LEN 30
#define FINANCE_ENTRY_COUNT 10

#define SHIP_MESSAGE_LEN SHIPNAME_LEN + SHIPSTATUS_LEN + 7

#define FINANCE_MESSAGE_LEN \
	((sizeof(long) * FINANCE_ENTRY_LEN) * FINANCE_ENTRY_COUNT) + 2

typedef struct {
	int max, current;
} Bar;

typedef struct {
	char name[SHIPNAME_LEN];
	char status[SHIPSTATUS_LEN];
	Bar hull, energy, armor;
} Ship;

typedef struct {
	char name[FINANCE_ENTRY_LEN];
	long value;
} FinanceEntry;

typedef enum {
	SHIP_UPDATE,
	FINANCE_UPDATE,
} MessageType;
