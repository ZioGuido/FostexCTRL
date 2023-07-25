////////////////////////////////////////////////////////////////////////////////////////////
// Fostex R8 Controller
// Connects to the SYNCHRO port of a Fostex R8 (or compatible) R2R tape recorder
// Controls transport functions via MIDI MMC (Midi Machine Control) or by
// means of an Alesis LRC controller.
//
// The Alesis LRC is based on a resistor network divider, each keypress engages
// a different resistor combination that results in different readings at the output
// jack. By reading this from an ADC input we can determine which key (or key combination)
// was depresset on the LRC.
//
// Code by: Guido Scognamiglio - www.GenuineSoundware.com
// Last update: Jult 2023
//
// Based on Arduino Leonardo (AT32u4) or clones. Can work with other CPU boards.
//


#include <USB-MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();
USBMIDI_CREATE_INSTANCE(0, USB_MIDI)


#define PIN_PLAY	4
#define PIN_FF		5
#define PIN_STOP	6
#define PIN_REW		9
#define PIN_REC		8
#define PIN_LIFTER	16

#define PIN_UD		7		// Tape count encoder pulse
#define PIN_PULSE	10		// Tape Direction, reads: LOW = forward, HIGH = backward

#define MMC_PLAY	0x02
#define MMC_DEFPLAY	0x03
#define MMC_FF		0x04
#define MMC_STOP	0x01
#define MMC_REW		0x05
#define MMC_REC		0x06

// Tape counter variables
volatile int PulseCount = 0;
int Direction = 0, SearchDirection = 0; // 1 = FORWARD, -1 = BACKWARD

// Alesis LRC reading variables
int prevLRC_value = 0;
auto LRC_Timer = millis();

enum LRC_Buttons { IDLE = 0, PLAY, STOP, RECORD, REWIND, FFWD, LOCATE1, LOCATE2, LOCATE3, LOCATE4, SETLOCATE, AUTOLOOP, AUTORECORD, REHARSE };
int LRC_Button = 0;
int LocateMemory[4], LocateDestination = 0;
bool Searching = false;
auto WindTimer = millis();


bool TestMargin(int test_val, int ref_val, int margin)
{
	return (test_val > ref_val - margin && test_val < ref_val + margin);
}

void DoLRC(int value)
{
	Serial.print("LRC Value: ");
	Serial.println(value);

		 if (TestMargin(value, 487, 4)) { LRC_Button = LRC_Buttons::PLAY;		Searching = false; PulseOut(PIN_PLAY);		}
	else if (TestMargin(value, 563, 4)) { LRC_Button = LRC_Buttons::FFWD;		Searching = false; PulseOut(PIN_FF);		}
	else if (TestMargin(value, 517, 4)) { LRC_Button = LRC_Buttons::STOP;		Searching = false; PulseOut(PIN_STOP);		}
	else if (TestMargin(value, 541, 4)) { LRC_Button = LRC_Buttons::REWIND;		Searching = false; PulseOut(PIN_REW);		}
	else if (TestMargin(value, 583, 4)) { LRC_Button = LRC_Buttons::RECORD;		Searching = false; PulseOut(PIN_REC);		}
	else if (TestMargin(value,  71, 4)) { LRC_Button = LRC_Buttons::REHARSE;	Searching = false; PulseOut(PIN_LIFTER);	}
	else if (TestMargin(value, 458, 4)) { LRC_Button = LRC_Buttons::RECORD;		Searching = false; PulseOutRec();			}

	else if (TestMargin(value, 114, 4)) { LRC_Button = LRC_Buttons::AUTORECORD;	PauseSearch(); }

	else if (TestMargin(value, 311, 4)) { LRC_Button = LRC_Buttons::SETLOCATE; }
	else if (TestMargin(value, 191, 4) && LRC_Button == LRC_Buttons::SETLOCATE) { SetLocate(0); }
	else if (TestMargin(value, 255, 4) && LRC_Button == LRC_Buttons::SETLOCATE) { SetLocate(1); }
	else if (TestMargin(value, 284, 4) && LRC_Button == LRC_Buttons::SETLOCATE) { SetLocate(2); }
	else if (TestMargin(value, 154, 4) && LRC_Button == LRC_Buttons::SETLOCATE) { SetLocate(3); }
	else if (TestMargin(value, 224, 4) && LRC_Button == LRC_Buttons::SETLOCATE) { PulseCount = 0; }	// reset tape counter (0 position)

	else if (TestMargin(value, 191, 4)) { LRC_Button = LRC_Buttons::LOCATE1; Locate(LocateMemory[0]); }
	else if (TestMargin(value, 255, 4)) { LRC_Button = LRC_Buttons::LOCATE2; Locate(LocateMemory[1]); }
	else if (TestMargin(value, 284, 4)) { LRC_Button = LRC_Buttons::LOCATE3; Locate(LocateMemory[2]); }
	else if (TestMargin(value, 154, 4)) { LRC_Button = LRC_Buttons::LOCATE4; Locate(LocateMemory[3]); }
	else if (TestMargin(value, 224, 4)) { LRC_Button = LRC_Buttons::AUTOLOOP; Locate(0); }

	else LRC_Button = LRC_Buttons::IDLE;
}

void ReadLRC()
{
	auto LRC_value = analogRead(A0);
	if (abs(prevLRC_value - LRC_value) > 10)
	{
		prevLRC_value = LRC_value;
		DoLRC(LRC_value);
	}
}

void AllHigh()
{
	digitalWrite(PIN_PLAY,	HIGH);
	digitalWrite(PIN_FF,	HIGH);
	digitalWrite(PIN_STOP,	HIGH);
	digitalWrite(PIN_REW,	HIGH);
	digitalWrite(PIN_REC,	HIGH);
	digitalWrite(PIN_LIFTER,HIGH);
}

void PulseOut(byte pin)
{
	AllHigh();
	digitalWrite(pin, LOW);
	delay(50);
	digitalWrite(pin, HIGH);
}

void PulseOutRec()
{
	// Press both PLAY & REC buttons to start recording armed tracks
	AllHigh();
	digitalWrite(PIN_REC,	LOW);
	digitalWrite(PIN_PLAY,	LOW);
	delay(50);
	digitalWrite(PIN_REC,	HIGH);
	digitalWrite(PIN_PLAY,	HIGH);
}

void SetLocate(byte loc)
{
	switch (loc)
	{
	case 0: LRC_Button = LRC_Buttons::LOCATE1; LocateMemory[0] = PulseCount; break;
	case 1: LRC_Button = LRC_Buttons::LOCATE2; LocateMemory[1] = PulseCount; break;
	case 2: LRC_Button = LRC_Buttons::LOCATE3; LocateMemory[2] = PulseCount; break;
	case 3: LRC_Button = LRC_Buttons::LOCATE4; LocateMemory[3] = PulseCount; break;
	}

	Serial.print("Set Locate "); Serial.print(loc); Serial.print(" at position "); Serial.println(PulseCount);
}

void Locate(int position)
{
	if (PulseCount == position)
		return;

	Searching = true;
	LocateDestination = position;

	Serial.print("Locate at position "); Serial.println(LocateDestination);

	AllHigh();
	SearchDirection = (LocateDestination > PulseCount) ? 1 : -1;
	digitalWrite((SearchDirection > 0) ? PIN_FF : PIN_REW, LOW);
}

void PauseSearch()
{
	AllHigh();
	digitalWrite(PIN_FF,	LOW); 
	digitalWrite(PIN_REW,	LOW);
	delay(50);
	digitalWrite(PIN_FF,	HIGH);
	digitalWrite(PIN_REW,	HIGH);
}

void handleSystemExclusive(byte* array, unsigned size)
{
	if (size != 6) return;
	if (array[3] != 0x06) return;

	switch (array[4])
	{
	case MMC_DEFPLAY:
	case MMC_PLAY:	PulseOut(PIN_PLAY	); break;
	case MMC_FF:	PulseOut(PIN_FF		); break;
	case MMC_STOP:	PulseOut(PIN_STOP	); break;
	case MMC_REW:	PulseOut(PIN_REW	); break;
	case MMC_REC:	PulseOut(PIN_REC	); break;
	}
}

void GetPulse() // ISR
{
	PulseCount += Direction;
}

void setup() 
{
	Serial.begin(115200);

	pinMode(PIN_PLAY,	OUTPUT);
	pinMode(PIN_FF,		OUTPUT);
	pinMode(PIN_STOP,	OUTPUT);
	pinMode(PIN_REW,	OUTPUT);
	pinMode(PIN_REC,	OUTPUT);
	pinMode(PIN_LIFTER,	OUTPUT);

	AllHigh();

	pinMode(PIN_PULSE, INPUT_PULLUP);
	pinMode(PIN_UD,		INPUT_PULLUP);

	// Setup MIDI
	MIDI.turnThruOff();
	MIDI.setHandleSystemExclusive(handleSystemExclusive);
	MIDI.begin(MIDI_CHANNEL_OMNI);

	USB_MIDI.turnThruOff();
	USB_MIDI.setHandleSystemExclusive(handleSystemExclusive);
	USB_MIDI.begin(MIDI_CHANNEL_OMNI);

	// Read the tape counter
	attachInterrupt(digitalPinToInterrupt(PIN_UD), GetPulse, FALLING);

	for (auto& m : LocateMemory) m = 0;
}

void loop() 
{
	// Read Midi inputs
	MIDI.read();
	USB_MIDI.read();

	// Always get tape direction
	Direction = digitalRead(PIN_PULSE) == LOW ? 1 : -1;

	// Read ADC for LRC input every 100 milliseconds
	if (millis() - LRC_Timer > 100)
	{
		ReadLRC();
		LRC_Timer = millis();
	}

	// Locate and stop when reached destination
	if (Searching)
	{
		// Slow down when approaching destination
		if (abs(LocateDestination - PulseCount) < 250)
		{
			// There's no way to ask the R8 to slow down like when keeping FF or RWD depressed on its own controller
			// so slowing down is achieved by pulsating the other direction wind button until the destination is reached
			auto oppositeDirectionPin = (SearchDirection > 0) ? PIN_REW : PIN_FF;
			if (millis() - WindTimer > 400)
			{
				WindTimer = millis();
				digitalWrite(oppositeDirectionPin, !digitalRead(oppositeDirectionPin));
				//Serial.print("PULSATING SLOW DOWN PIN: "); Serial.print(oppositeDirectionPin); Serial.print(" STATE: "); Serial.println(digitalRead(oppositeDirectionPin));
			}
		}

		// Stop when destination is reached
		if ((SearchDirection > 0 && PulseCount >= LocateDestination) || (SearchDirection < 0 && PulseCount <= LocateDestination))
		{
			PulseOut(PIN_STOP);
			delay(50);
			Serial.print("Was searching: "); Serial.print(LocateDestination); Serial.print(" Found: "); Serial.println(PulseCount);
			Searching = false;
			PulseCount = LocateDestination;
		}
	}
}
