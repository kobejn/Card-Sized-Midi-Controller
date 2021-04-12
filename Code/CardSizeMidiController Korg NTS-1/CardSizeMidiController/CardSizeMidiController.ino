// Card-sized midi controller Firmware for KORG NTS-1
// Replaces LEVEL / PAN / MOD modes for additional CC

#define SW0 2	
#define SW1 4
#define SW2 7
#define SW3 8
#define SW4 12
#define SW5 13
#define POT0 A0
#define POT1 A1
#define POT2 A2
#define POT3 A3
#define POT4 A4
#define POT5 A5
#define LED0 3
#define LED1 5
#define LED2 6
#define LED3 9
#define LED4 10
#define LED5 11
#define SIX 6

//Pots
#define BUFFER 11
#define BUFFERSIZE 22 // BUFFER * sizeof(int)
int logResponse[128] =  {0, 18, 29, 37, 42, 47, 51, 55, 58, 61, 63, 66, 68, 70, 71, 
						73, 75, 76, 78, 79, 80, 82, 83, 84, 85, 86, 87, 88, 89, 90, 
						91, 91, 92, 93, 94, 95, 95, 96, 97, 97, 98, 99, 99, 100, 
						100, 101, 102, 102, 103, 103, 104, 104, 105, 105, 106, 106, 
						107, 107, 108, 108, 108, 109, 109, 110, 110, 111, 111, 111, 
						112, 112, 112, 113, 113, 114, 114, 114, 115, 115, 115, 116, 
						116, 116, 117, 117, 117, 118, 118, 118, 118, 119, 119, 119, 
						120, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 123, 
						123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 
						126, 126, 126, 126, 127, 127, 127, 127, 127, 128, 128, 128};
int index = 0; //index is shared
int potRead[BUFFER][SIX];
int potPort[SIX] = {POT0, POT1, POT2, POT3, POT4, POT5};
int currentMedian[SIX] = {0, 0, 0, 0, 0, 0};
int previousMedian[SIX] = {0, 0, 0, 0, 0, 0};
int sortBuff[BUFFER];

//Leds
int ledBrightness = 10; //3.9%
int ledPort[SIX] = {LED0, LED1, LED2, LED3, LED4, LED5};
int ledStatus[SIX] = {ledBrightness, 0, 0, 0, 0, 0};

//Switches
int swPort[SIX] = {SW0, SW1, SW2, SW3, SW4, SW5};
int currentButtonState[SIX] = {0, 0, 0, 0, 0, 0};
int previousButtonState[SIX] = {0, 0, 0, 0, 0, 0};
int pressedButton[SIX] = {0, 0, 0, 0, 0, 0};

//CC banks
int CC1[SIX] = {53, 54, 55, 24, 26, 1}; 		//OSC. TYPE, SHAPE, ALT, OSC. LFO RATE, DEPTH 
int CC2[SIX] = {42, 43, 44, 45, 46, 1};			//FILTER TYPE, CUTOFF, RESONANCE, FILTER SWEEP DEPTH, RATE 
int CC3[SIX] = {14, 16, 19, 20, 21, 1};			//EG TYPE, ATTACK, RELEASE, TREMOLLO DEPTH, RATE
int CC4[SIX] = {88, 28, 29, 117, 118, 119};		//MOD. TYPE, MOD. TIME, DEPTH, ARP. PATTERN, INTERVALS, LENGTH
int CC5[SIX] = {89, 30, 31, 33, 1, 1};			//DELAY TYPE, DELAY TIME, DEPTH, MIX
int CC6[SIX] = {90, 34, 35, 36, 1, 1};			//REVERB TYPE, REVERB TIME, DEPTH, MIX 

//general
int mode = 1;
int channel = 0;
int randomness = 0;
int logness = 0;
int preset = 0;
int secondPage = 0;

//function prototypes
void updateLeds(int * leds, int * stat);
void allLedsOn(int * stat);
void allLedsOff(int * stat);
void channel2led(int chan);
void updateButtons(int * buttons, int * current, int * previous);
void pushedButtons(int * buttons, int * current, int * previous);
int median(int * a,int n);
void swap(int *p,int *q);
void copyRow(int *destMatrix, int srcMatrix[][SIX], int ind);
void midiCCsend(int channel, int cmd, int value);
void flash(int led, int brightness, int mill, int times);

void setup() {
	delay(1000);
	Serial.begin(31250);
	for(int i = 0; i < SIX; i++){
		pinMode(swPort[i], INPUT_PULLUP);
		pinMode(ledPort[i], OUTPUT);
	}
	updateLeds(ledPort, ledStatus); 
}

void loop() {
	updateButtons(swPort, currentButtonState, previousButtonState);
	pushedButtons(pressedButton, currentButtonState, previousButtonState);
	int settingFlag = 0;
	
	//double button presses
	// 1+2 Randomness
	if(!currentButtonState[0] && !currentButtonState[1]){
		randomness = map(analogRead(potPort[5]), 0, 1023, 0, 127);
		settingFlag = 1;
	}
	// 3+4 Logness
	if(!currentButtonState[2] && !currentButtonState[3]){
		logness = map(analogRead(potPort[5]), 0, 1023, 0, 255);
		settingFlag = 1;
	}
	// 5+6 LED brightness
	if(!currentButtonState[4] && !currentButtonState[5]){
		ledBrightness = map(analogRead(potPort[5]), 0, 1023, 0, 255);
		channel2led(5);
		updateLeds(ledPort, ledStatus);  
		settingFlag = 1;
	}
	
	//single button presses
	if(!settingFlag){
		for (int i = 0; i < SIX; i++){
			if(pressedButton[i]){
				mode = i;
				allLedsOff(ledStatus);
				channel2led(mode);
				updateLeds(ledPort, ledStatus);
			}
		}

		//midi operations
		for (int i = 0; i < SIX; i++){
			previousMedian[i] = currentMedian[i];
			potRead[index][i] = analogRead(potPort[i]);
			copyRow(sortBuff, potRead, i);
			currentMedian[i] = map(median(sortBuff, BUFFER), 0, 1023, 0, 127);
			if (currentMedian[i] != previousMedian[i]){
				int finalValue; 
				int variation = random(-randomness, randomness);
				finalValue = (currentMedian[i] * (255 - logness))/255 + (logResponse[currentMedian[i]] * logness)/255 + variation;
				if(variation > 0 && finalValue > 127){
					finalValue = 127;
				}
				if(variation < 0 && finalValue < 0){
					finalValue = 0;
				}
				switch (mode){
					case 0:
						midiCCsend(channel, CC1[i], finalValue);
					break;
					case 1:
						midiCCsend(channel, CC2[i], finalValue);
					break;
					case 2:
						midiCCsend(channel, CC3[i], finalValue);
					break;
					case 3:
						midiCCsend(channel, CC4[i], finalValue);
					break;
					case 4:
						midiCCsend(channel, CC5[i], finalValue);
					break;
					case 5:
						midiCCsend(channel, CC6[i], finalValue);
					break;
				}
			}
		}
		index = (index +1) % BUFFER;
		delay(5);
	}
}

void updateLeds(int * leds, int * stat){
	int i;
	for(i = 0; i < SIX; i++){
		analogWrite(leds[i], stat[i]);
	}
}

void allLedsOn(int * stat){
	int i;
	for(i = 0; i < SIX; i++){
		stat[i] = ledBrightness;
	}
}

void allLedsOff(int * stat){
	int i;
	for(i = 0; i < SIX; i++){
		stat[i] = 0;
	}
}

void channel2led(int chan){
	if (chan < 6){
		ledStatus[chan]= ledBrightness;
	}	
	else{
		switch(chan){
			case 6:
				ledStatus[0] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;

			case 7:
				ledStatus[1] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;

			case 8:
				ledStatus[2] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;

			case 9:
				ledStatus[3] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;
			
			case 10:
				ledStatus[4] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;

			case 11:
				ledStatus[0] = ledBrightness;
				ledStatus[4] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;
			
			case 12:
				ledStatus[1] = ledBrightness;
				ledStatus[4] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;

			case 13:
				ledStatus[2] = ledBrightness;
				ledStatus[4] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;
			
			case 14:
				ledStatus[3] = ledBrightness;
				ledStatus[4] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;

			case 15:
				ledStatus[0] = ledBrightness;		
				ledStatus[3] = ledBrightness;
				ledStatus[4] = ledBrightness;
				ledStatus[5] = ledBrightness;
			break;
		}
	}
}

void updateButtons(int * buttons, int * current, int * previous){
	for(int i = 0; i < SIX; i++){
		previous[i] = current[i];
		current[i] = digitalRead(buttons[i]);
	}
}

void pushedButtons(int * buttons, int * current, int * previous){
	for(int i = 0; i < SIX; i++){
		(previous[i] && !current[i]) ? buttons[i] = 1 : buttons[i] = 0;
	}
}

int median(int * a,int n) { 
   int i,j;
   for(i = 0;i < n-1;i++) {
      for(j = 0;j < n-i-1;j++) {
         if(a[j] > a[j+1])
            swap(&a[j],&a[j+1]);
      }
   }
   n = (n+1) / 2 - 1;
   return a[n];
}

void swap(int *p,int *q) {
   int t;
   t=*p; 
   *p=*q; 
   *q=t;
}

void copyRow(int *destMatrix, int srcMatrix[][SIX], int ind){
	for (int i = 0; i < BUFFER; i++){
		destMatrix[i] = srcMatrix[i][ind];
	}
}

void midiCCsend(int channel, int cmd, int value) {
	Serial.write(0xb0 | channel);
	Serial.write(cmd);
	Serial.write(value);
}

void flash(int led, int brightness, int mill, int times){
	for (int ind = 0; ind < times; ind++){
		analogWrite(led, brightness);
		delay(mill);
		digitalWrite(led, 0);
		delay(mill);	
	}
}
	
	
