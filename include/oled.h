#define ALL	1
#define PAGE	2

class OLED {
public:
	OLED(uint8_t _a, uint8_t _b) { };
	void print(char *) { } ;
	void setCursor(uint8_t _a, uint8_t _b) { } ;
	void clear(uint8_t _a) {};
	void display() { };
	void begin() {};
	void setFontType(uint8_t _a) {};
	void drawBitmap(uint8_t *_a) {};
};
