/*
������� � ������ ���������� ������� ��������.

2015-12-19, Wyfinger
*/

#include <EEPROM.h>

#define Q1 13       // ������ ������ �����
#define Q2 12
#define Q3 11
#define Q4 10
#define Q5 9
#define Q6 8
#define Q7 7
#define Q8 6

#define Buttons    A0  // ������ (��� ������ ����� �������� ���������� �� ����� ��� ����)
#define BtnCount   6 

#define Gercone   A1 // ������ ������������, 0 ���� �������
#define Beeper    A2 // ������� �� ���������� �����������

#define SeriesA  5  // ��������
#define SeriesB  4

#define Clicker  3  // ����, �� ������� ����� �������������� ����� �� 74HC164 (�.�. �� ����������� ������ �� ������������ ��� � �����)
#define DSPReset 2  

#define PulseMeter A3 // ���� �� ������� ����� ������ ������, � ������ ������ ������ ���������� ������ ���� ������, �.�.
// ����� ������� ������� ��� � ��������, ���� ��� ������� ������, ������� ���� ���������� �������� �� 0 �� 5V, ���������
// ����� �������� ���� ����� ����� ��������� �����, ���� ���� ��������� ���������.

#define Dights   8 // �������� � ������ �����

#define SerialSpeed   9600 // �������� ������ ������� � ������� �������


const byte Q[Dights] = { Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8 };

// ��������� �������, ��������*10 �� ������, �������� � ���� �����, ��� �����
String P[] = {
	"2<FFFFF<<FFFFF<2",
	"22<<FFPPZZZPFFFPZdPF<22",
	"8",
	"ddd",
	"",
	"",
	"",
	"",
	"",
	"000000000000000000000"
};

byte PPos = 0;  // ����� ������ � ��������� ����������
long CheckProgramTime = 0; //

const float MinSpeed = 1.0;
const float MaxSpeed = 12.0;

byte LineA[Dights]; // ����� �������� �� �������, ������ � ������ �����
byte LineB[Dights];
byte CurrDight = 0; // ������� ������������ ������

byte ProgramNo = 0;  // ��������� �������������� ��������� ����������, ����������� � EEPROM

enum DM { dmStop, dmProgramSelect, dmManualRun, dmProgramRun, dmError };
DM DisplayMode;

bool AltView = false;  // ����� ��������������� ����������� - ���������� ������� 'SET', ������ �������
// ������������ ���� ������� ������� �������, � ���� �������� � ������ dmProgramRun - ������
// ������� � ��������� ������������ ���������� ����� � ��������� � ������������ � ������� ����������


float CurrSpeed = 0;    // ������� ��������
int CurrTime = 0;        // �����
float CurrCalorie = 0;     // ����������� �������
float CurrDistance = 0;  // ���������
int CurrPulse = 0;       // �����
int CurrAngle = 0;       // ���� ������� �������, � ��������

long StartTime = 0;

long ButtonClickTime;  // ����� ���������� ������� �� ������, ��� ������ �� ������������ �������
bool ButtonStatus;    // true ���� ���� ���� ������ ������ � false, ���� ��� ������
long BeeperTime;       // ����� ��������� �������, ����� ���������� ������������ �����

enum EC { ecNormal = 0, ecGercone };
EC ErrorCode;

bool PulseStatus;  // ��������� ��������� � ������� ������, ����� ����������� �����
long PulseLastTime = 0;    // ����� ���������� ����� ������
byte PulseAveraging[10];   // ���� ���������� ������

long CheckTimeLast = 0;
long CheckSpeedTimeMark = 0; // ����� ��������� �������� �������� �� ������� ������

String ReciveBuff = "";  // �����, ������������� ��������� �� �������� �����

void setup() {
	// ������������� �������
	pinMode(Q1, OUTPUT);
	pinMode(Q2, OUTPUT);
	pinMode(Q3, OUTPUT);
	pinMode(Q4, OUTPUT);
	pinMode(Q5, OUTPUT);
	pinMode(Q6, OUTPUT);
	pinMode(Q7, OUTPUT);
	pinMode(Q8, OUTPUT);
	pinMode(SeriesA, OUTPUT);
	pinMode(SeriesB, OUTPUT);
	pinMode(Clicker, OUTPUT);
	pinMode(DSPReset, OUTPUT);
	// �������� ��� ����������
	digitalWrite(Q1, HIGH);
	digitalWrite(Q2, HIGH);
	digitalWrite(Q3, HIGH);
	digitalWrite(Q4, HIGH);
	digitalWrite(Q5, HIGH);
	digitalWrite(Q6, HIGH);
	digitalWrite(Q7, HIGH);
	digitalWrite(Q8, HIGH);
	// ����� ���������� ����� ������ � �������
	pinMode(Buttons, INPUT_PULLUP);
	pinMode(Gercone, INPUT_PULLUP);
	pinMode(Beeper, OUTPUT);
	// ��������� ������������ ��������� ����������
	ProgramNo = EEPROM.read(0);
	if (ProgramNo >= sizeof(P))  ProgramNo = sizeof(P)-1;
	// ���������� �������
	PrepareDisplay();
	// ����� � ������� �������
	Serial.begin(SerialSpeed);
	// �������� ���� ���������� ������ ������
	for (int i = 0; i<sizeof(PulseAveraging); i++)
	{
		PulseAveraging[i] = 0;
	}
}

// �������������� ����� � ������� ����� ��� 7 ���������
byte EncodeNum(byte Num)
{
	switch (Num)
	{
	case 0: return 0x3F;
	case 1: return 0x06;
	case 2: return 0x5B;
	case 3: return 0x4F;
	case 4: return 0x66;
	case 5: return 0x6D;
	case 6: return 0x7D;
	case 7: return 0x07;
	case 8: return 0x7F;
	case 9: return 0x6F;
	}
}

// ��������� ����� � �����
byte AddDot(byte Num)
{
	return Num | B10000000;
}

// ���������� ����� �� ��������
void ShowNum(byte NumA, byte NumB)
{
	// ���������� ��������
	digitalWrite(DSPReset, LOW);
	digitalWrite(DSPReset, HIGH);
	// ����������� ������ ��������
	for (int i = 7; i >= 0; i--)
	{
		digitalWrite(SeriesA, !bitRead(NumA, i));
		digitalWrite(SeriesB, !bitRead(NumB, i));
		// ������
		digitalWrite(Clicker, HIGH);
		digitalWrite(Clicker, LOW);
	}
}

// �������� �������� �������, >=0 - �� �������, <0 - ����� �������
byte GetDight(float Num, int Dight)
{
	return (int)(Num*pow(10, -Dight)) % 10;
}

// ���������� ������ ��� ������� � ������ ������ ��������� ����������
void PrepareDisplayInProgramSelect()
{
	// ��������� ������ 'P' � ����� ���������, ������ ��������,
	// � ����� ����� � ���������, ���������� � ���������
	int ProgTime = ProgramTime(P[ProgramNo]);
	float ProgDistance = ProgramDistance(P[ProgramNo]);
	// ����� ���������
	LineA[0] = 0x73;   // ������ 'P'
	LineA[1] = GetDight(ProgramNo + 1, 1);
	if (LineA[1] > 0)  LineA[1] = EncodeNum(LineA[1]);
	LineA[2] = EncodeNum(GetDight(ProgramNo + 1, 0));
	// �����
	LineA[6] = EncodeNum(GetDight(ProgTime / 60, 1));
	LineA[7] = AddDot(EncodeNum(GetDight(ProgTime / 60, 0)));
	LineB[6] = AddDot(EncodeNum(GetDight(ProgTime % 60, 1)));
	LineB[7] = EncodeNum(GetDight(ProgTime % 60, 0));
	// ���������
	if (ProgDistance < 1)  // �� 1 �� ������� ��� ����� ����� �������
	{
		LineA[3] = AddDot(EncodeNum(GetDight(ProgDistance, 0)));
		LineA[4] = EncodeNum(GetDight(ProgDistance, -1));
		LineA[5] = EncodeNum(GetDight(ProgDistance, -2));
	}
	else {           // � ������ - ������ ����
		LineA[3] = GetDight(ProgDistance, 1);
		if (LineA[3] > 0)  LineA[3] = EncodeNum(LineA[3]);
		LineA[4] = AddDot(EncodeNum(GetDight(ProgDistance, 0)));
		LineA[5] = EncodeNum(GetDight(ProgDistance, -1));
	}
	// ������� ��������� �����
	LineB[1] = 0;
	LineB[2] = 0;
	LineB[3] = 0;
	LineB[4] = 0;
	LineB[5] = 0;
}

// ���������� ������ ��� ������� � ������ ������
void PrepareDisplayInError()
{
	// ��������� ������ 'E' � ����� ������ � ����� ��������
	LineA[0] = 0x79;   // ������ 'E'
	LineA[1] = 0;
	LineA[2] = EncodeNum(ErrorCode);
	// ������� ��������� �����
	for (int i = 3; i<Dights; i++)
	{
		LineA[i] = 0;
		LineB[i] = 0;
	}
	for (int i = 0; i<3; i++)
	{
		LineB[i] = 0;
	}
}

// ���������� ������ ��� ������� � ������ ������
void PrepareDisplayInAltView()
{
	// ��������
	LineA[0] = GetDight(CurrSpeed, 1);
	if (LineA[0] > 0)  LineA[0] = EncodeNum(LineA[0]);  // �� ���������� ���� � ������
	LineA[1] = AddDot(EncodeNum(GetDight(CurrSpeed, 0)));
	LineA[2] = EncodeNum(GetDight(CurrSpeed, -1));

	float Distance = CurrDistance;
	int Time = CurrTime;
	if (DisplayMode == dmProgramRun) // ����� ������ �� ��������� - ���������� 
	{
		// ���������
		Distance = ProgramDistance(P[ProgramNo]) - CurrDistance;
		// �����, ������
		Time = ProgramTime(P[ProgramNo]) - CurrTime;
	}
	// ��������� 
	if (Distance < 1)  // �� 1 �� ������� ��� ����� ����� �������
	{
		LineA[3] = AddDot(EncodeNum(GetDight(Distance, 0)));
		LineA[4] = EncodeNum(GetDight(Distance, -1));
		LineA[5] = EncodeNum(GetDight(Distance, -2));
	}
	else {           // � ������ - ������ ����
		LineA[3] = GetDight(Distance, 1);
		if (LineA[3] > 0)  LineA[3] = EncodeNum(LineA[3]);
		LineA[4] = AddDot(EncodeNum(GetDight(Distance, 0)));
		LineA[5] = EncodeNum(GetDight(Distance, -1));
	}
	// �����, ������ 
	LineA[6] = EncodeNum(GetDight(Time / 60, 1));
	LineA[7] = EncodeNum(GetDight(Time / 60, 0));
	if (Time % 2 == 0)  LineA[7] = AddDot(LineA[7]);
	// � ���� ������� ���������� ���� �������
	LineB[0] = GetDight(CurrAngle, 1);
	if (LineB[0] > 0)  LineB[0] = EncodeNum(LineB[0]);
	LineB[1] = EncodeNum(GetDight(CurrAngle, 0));
	LineB[2] = 0x63;   // ������ ������� 
	// �����
	LineB[3] = GetDight(CurrPulse, 2);
	if (LineB[3] > 0)  LineB[3] = EncodeNum(LineB[3]);
	LineB[4] = GetDight(CurrPulse, 1);
	if (LineB[4] > 0)  LineB[4] = EncodeNum(LineB[4]);
	LineB[5] = EncodeNum(GetDight(CurrPulse, 0));
	// �����, �������
	LineB[6] = EncodeNum(GetDight(Time % 60, 1)); // �� ������ 59, ���� �������
	LineB[7] = EncodeNum(GetDight(Time % 60, 0));
	if (Time % 2 == 0)  LineB[6] = AddDot(LineB[6]);
}

// ���������� ������ �������
void PrepareDisplay()
{
	if (ErrorCode > ecNormal)
	{
		PrepareDisplayInError();
		return;
	}
	// ���� ������� � ������ ������ ��������� ���������� - ��������� ���� �� �������
	if (DisplayMode == dmProgramSelect)
	{
		PrepareDisplayInProgramSelect();
		return;
	}
	// �������������� ����� (���� ������� � ���������� �����)
	if (AltView)
	{
		PrepareDisplayInAltView();
		return;
	}

	// ��������
	LineA[0] = GetDight(CurrSpeed, 1);
	if (LineA[0] > 0)  LineA[0] = EncodeNum(LineA[0]);  // �� ���������� ���� � ������
	LineA[1] = AddDot(EncodeNum(GetDight(CurrSpeed, 0)));
	LineA[2] = EncodeNum(GetDight(CurrSpeed, -1));
	// ���������
	if (CurrDistance < 1)  // �� 1 �� ������� ��� ����� ����� �������
	{
		LineA[3] = AddDot(EncodeNum(GetDight(CurrDistance, 0)));
		LineA[4] = EncodeNum(GetDight(CurrDistance, -1));
		LineA[5] = EncodeNum(GetDight(CurrDistance, -2));
	}
	else {           // � ������ - ������ ����
		LineA[3] = GetDight(CurrDistance, 1);
		if (LineA[3] > 0)  LineA[3] = EncodeNum(LineA[3]);
		LineA[4] = AddDot(EncodeNum(GetDight(CurrDistance, 0)));
		LineA[5] = EncodeNum(GetDight(CurrDistance, -1));
	}
	// �����, ������
	LineA[6] = EncodeNum(GetDight(CurrTime / 60, 1));
	LineA[7] = EncodeNum(GetDight(CurrTime / 60, 0));
	if (CurrTime % 2 == 0)  LineA[7] = AddDot(LineA[7]);
	// �������
	LineB[0] = GetDight(CurrCalorie, 2);
	if (LineB[0] > 0)  LineB[0] = EncodeNum(LineB[0]);
	LineB[1] = GetDight(CurrCalorie, 1);
	if (LineB[1] > 0)  LineB[1] = EncodeNum(LineB[1]);
	LineB[2] = EncodeNum(GetDight(CurrCalorie, 0));
	// �����
	LineB[3] = GetDight(CurrPulse, 2);
	if (LineB[3] > 0)  LineB[3] = EncodeNum(LineB[3]);
	LineB[4] = GetDight(CurrPulse, 1);
	if (LineB[4] > 0)  LineB[4] = EncodeNum(LineB[4]);
	LineB[5] = EncodeNum(GetDight(CurrPulse, 0));
	// �����, �������
	LineB[6] = EncodeNum(GetDight(CurrTime % 60, 1)); // �� ������ 59, ���� �������
	LineB[7] = EncodeNum(GetDight(CurrTime % 60, 0));
	if (CurrTime % 2 == 0)  LineB[6] = AddDot(LineB[6]);
}

// ������� ������� � ������������� ���������
int ProgramTime(String Program)
{
	return  60 * Program.length();
}

// ������� ��������� � ������������� ���������
float ProgramDistance(String Program)
{
	float Summ = 0.0001;
	for (int i = 0; i <= Program.length(); i++)
	{
		Summ = Summ + Program[i];   // ��/�, ����� �� �������
	}
	return Summ / 600.0;
}

// ���������� ����������� �������
void UpdateDisplay()
{
	// ��������� ���������� ������
	digitalWrite(Q[CurrDight], HIGH);
	// ������������ � ������� ��������� ������
	CurrDight++;
	if (CurrDight == Dights)  CurrDight = 0;
	ShowNum(LineA[CurrDight], LineB[CurrDight]);
	// �������� ����� ������
	digitalWrite(Q[CurrDight], LOW);
}

// �������� ��������� �� ������� ������
void SendPacket(char Code, int Value)
{
	byte buff[10];

	buff[0] = 'D';          // ��� ��������, ��� ����� �� �������� ������ �� �������
	buff[1] = Code;
	// ������� �������� � ���� �����
	buff[2] = (byte)(Value*pow(10, -3)) % 10;
	buff[3] = (byte)(Value*pow(10, -2)) % 10;
	buff[4] = (byte)(Value*pow(10, -1)) % 10;
	buff[5] = (byte)(Value*pow(10, -0)) % 10;
	buff[6] = buff[0] ^ buff[1] ^ buff[2] ^ buff[3] ^ buff[4] ^ buff[5];
	// ����������� �� � ������
	buff[2] = String(buff[2], DEC)[0];
	buff[3] = String(buff[3], DEC)[0];
	buff[4] = String(buff[4], DEC)[0];
	buff[5] = String(buff[5], DEC)[0];
	String Hex = "0" + String(buff[6], HEX);
	Hex = Hex.substring(Hex.length() - 2);
	buff[6] = Hex[0];
	buff[7] = Hex[1];
	// ������� ������
	buff[8] = '\r';
	buff[9] = '\n';
	Serial.write(buff, sizeof(buff));
}

// �������� ��������� ��������� �� �������� ������
void RecivePacket()
{
	if (Serial.available() > 0)
	{
		char newChar = Serial.read();
		if (newChar != '\n')
		{
			ReciveBuff = ReciveBuff + newChar;
		}
		else // ����������� ������, �������� ��� �������� �����
		{
			char Direction = ReciveBuff[0];  // ����������� �������� ������, ������ ���� 'P'
			char Code = ReciveBuff[1];
			int Value = String(String(ReciveBuff[2]) + String(ReciveBuff[3]) + String(ReciveBuff[4]) + String(ReciveBuff[5])).toInt();

			// ��� ��� ���������� ���� ����� �������������� ���� ���� HEX -> Byte
			String hexCrc = "0x" + String(ReciveBuff[6]) + String(ReciveBuff[7]);
			byte Crc = (int)strtol(&hexCrc[0], NULL, 16);
			byte RealCrc = Direction ^ Code ^ (ReciveBuff[2] - 0x30) ^ (ReciveBuff[3] - 0x30) ^ (ReciveBuff[4] - 0x30) ^ (ReciveBuff[5] - 0x30);

			if ((Direction == 'P') && (Crc == RealCrc))  // ������� � ������ !
			{
				switch (Code)
				{
				case 'A':    // ������������ �������� ���� ������� �������, 
				{
								 CurrAngle = Value;
				}
				case 'E':    // �����-�� ������, �������� ��������, ���� ������ ����������� � ���� �� ������
				{
								 // ***
				}
				}
			}
			ReciveBuff = "";
		}
	}
}

// ���������� �� ������� ������ ������� ��������� ���� (��� ������, �������� ���� ����� ��������)
void SendError()
{
	SendPacket('E', 1);  // ������
}

// ��������
void Beep()
{
	if (ButtonStatus == false)  // ������� ����� ������ ���� ������ �� ������, �.�. ������ � ������ �������,
	{                         // �� �� ��� ������������ �������� (��� ���������� ������)
		BeeperTime = millis();
		digitalWrite(Beeper, HIGH);
	}
}

// ��������� �������
void CheckBeeper()
{
	if ((millis() - BeeperTime) > 100)
	{
		digitalWrite(Beeper, LOW);
	}
}

//** ��������� ������� ������

void ButtonReset()
{
	if ((DisplayMode == dmProgramSelect) || (DisplayMode == dmStop))
	{
		DisplayMode = dmStop;
		CurrSpeed = 0;
		CurrTime = 0;
		CurrCalorie = 0;
		CurrDistance = 0;
		Beep();
	}
}

void ButtonUp()
{
	if ((DisplayMode == dmStop) || (DisplayMode == dmManualRun) || (DisplayMode == dmProgramRun))
	{
		if (CurrSpeed < MinSpeed)
		{
			CurrSpeed = MinSpeed;
			StartTime = millis();
		}
		else {
			CurrSpeed = CurrSpeed + 0.1;
		}
		if (CurrSpeed > MaxSpeed)  CurrSpeed = MaxSpeed;
		DisplayMode = dmManualRun;
		Beep();
	}
}

void ButtonDown()
{
	if ((DisplayMode == dmStop) || (DisplayMode == dmManualRun) || (DisplayMode == dmProgramRun))
	{
		if (CurrSpeed > MinSpeed)  CurrSpeed = CurrSpeed - 0.1;
		if (CurrSpeed < MinSpeed)  CurrSpeed = MinSpeed;
		if (CurrSpeed != 0) DisplayMode == dmManualRun;

		Beep();
	}
}

void ButtonStartStop()
{
	if (DisplayMode == dmStop)  // ���� � ������ ��������� - �������� ���������� � ������ ������ � ����������� ��������
	{
		if (CurrSpeed < MinSpeed)
		{
			CurrSpeed = MinSpeed;
			DisplayMode = dmManualRun;
			StartTime = millis();
		}
		StartTime = millis();
		Beep();
		return;
	}
	if (DisplayMode == dmProgramSelect) // ���� � ������ ������ ��������� - �������� ������ �� ���������
	{
		DisplayMode = dmProgramRun;
		StartTime = millis();
		PPos = 0;
		CurrSpeed = (float)P[ProgramNo][PPos] / 10.0;
		Beep();
		return;
	}
	if ((DisplayMode == dmManualRun) || (DisplayMode == dmProgramRun)) // ���� � ������ ���������� - ���������
	{
		DisplayMode = dmStop;
		CurrSpeed = 0; // �������� ��������, �� �� ��������� ����������
		Beep();
		return;
	}
}

void ButtonSelect()
{
	// ���� ��������� � ������ ������ ��������� - �������� ��������� ���������
	if (DisplayMode == dmProgramSelect)
	{
		ProgramNo++;
		if (ProgramNo >= (sizeof(P) / sizeof(P[0])))  ProgramNo = 0;
		EEPROM.write(0, ProgramNo);
		Beep();
	}
	// ������� � ����� ������ ��������� ����� ������ �������� � ������ ��������
	if (DisplayMode == dmStop)
	{
		DisplayMode = dmProgramSelect;
		Beep();
	}
}

void ButtonSet()
{
	if ((DisplayMode != dmProgramSelect) && (DisplayMode != dmError))
	{
		AltView = !AltView;
		Beep();
	}
}

// �������� ������� �� ������ ����������
void CheckButtons()
{
	if (millis() - ButtonClickTime > 100)
	{
		bool NewButtonStatus = false;

		int maxVal = 1024;               // ������������ �������� ��� (10 ���)
		int stepVal = maxVal / (BtnCount); // ��� �������� ����������
		int halfVal = stepVal / 2;       // ��� ��� ������ �� ����� ���������� �����������

		int Val = analogRead(Buttons);

		if ((Val > 0 * stepVal - halfVal) && (Val < 0 * stepVal + halfVal))
		{
			NewButtonStatus = true;
			ButtonStartStop();
		}
		if ((Val > 1 * stepVal - halfVal) && (Val < 1 * stepVal + halfVal))
		{
			NewButtonStatus = true;
			ButtonSet();
		}
		if ((Val > 2 * stepVal - halfVal) && (Val < 2 * stepVal + halfVal))
		{
			NewButtonStatus = true;
			ButtonDown();
		}
		if ((Val > 3 * stepVal - halfVal) && (Val < 3 * stepVal + halfVal))
		{
			NewButtonStatus = true;
			ButtonSelect();
		}
		if ((Val > 4 * stepVal - halfVal) && (Val < 4 * stepVal + halfVal))
		{
			NewButtonStatus = true;
			ButtonUp();
		}
		if ((Val > 5 * stepVal - halfVal) && (Val < 5 * stepVal + halfVal))
		{
			NewButtonStatus = true;
			ButtonReset();
		}
		// ������� ������� ������
		if ((NewButtonStatus == false) && (ButtonStatus == true))
		{
			//
		}
		ButtonStatus = NewButtonStatus;
		ButtonClickTime = millis();
	}
}

// ���� ��������� � ������ ������ �� ��������� - ��� � ������ ��������� � ���������� �����
void CheckProgram()
{
	if (DisplayMode == dmProgramRun)
	{
		if ((CurrTime % 60 == 0) && (CheckProgramTime % 60 == 59))
		{
			PPos++;
			if (PPos >= P[ProgramNo].length()) // ��������� ���������� ��������
			{
				DisplayMode = dmStop;
				CurrSpeed = 0;
				CurrDistance = ProgramDistance(P[ProgramNo]);
				PPos = 0;
				Beep();
			}
			else {
				CurrSpeed = (float)P[ProgramNo][PPos] / 10.0;
			}
		}
		CheckProgramTime = CurrTime;
	}
}

// ���� ������ �� ������� - ���������� ������ � ��������� ����� (� ������ ����)
void CheckGercone()
{
	if (digitalRead(Gercone))
	{
		DisplayMode = dmError;
		ErrorCode = ecGercone;
		CurrSpeed = 0;
		CurrTime = 0;
		CurrCalorie = 0;
		CurrDistance = 0;
		SendError();  // ��� ��������� ����� � �������� ����
	}
	else {
		if ((DisplayMode == dmError) && (ErrorCode > ecNormal))
		{
			DisplayMode = dmStop;  /// TODO: ����� ������ ������ ��������� ���������
			ErrorCode = ecNormal;
		}
	}
}

// ������� ������� ������
void CheckPulse()
{
	// ����� ���������� ����� - ��� "����" ������
	bool Status = digitalRead(PulseMeter);
	if ((Status == false) && (PulseStatus == true)) // ���������� ����� 
	{
		int PulsePeriod = millis() - PulseLastTime;  // ������ ��������� ����������
		if ((PulsePeriod > 240) && (PulsePeriod < 3000))   // ����� �����-�� ������, �������� (����� �� 20 �� 250 ��/���)
		{
			long PulseRaw = 60000 / PulsePeriod;  // ���������� ����� �� ������� ����� ����� �������
			for (int i = sizeof(PulseAveraging)-1; i >= 1; i--)   // �������� ������ � ���� ����������
			{
				PulseAveraging[i] = PulseAveraging[i - 1];
			}
			PulseAveraging[0] = PulseRaw;
			int PulseSum = 0;
			int PulseCount = 0;
			for (int i = 0; i<sizeof(PulseAveraging); i++)
			{
				if (PulseAveraging[i] > 0)
				{
					PulseSum = PulseSum + PulseAveraging[i];
					PulseCount++;
				}
			}
			CurrPulse = PulseSum / PulseCount;
			PulseLastTime = millis();
		}
	}
	PulseStatus = Status;
}

// �������� ������������� �������, ���������� ��������� � ����������� �������
void CheckTime()
{
	if ((DisplayMode == dmManualRun) || (DisplayMode == dmProgramRun))
	{
		CurrTime = (millis() - StartTime) / 1000;
		// ��� � ������� ��������� ����� �������� ���������� ��������� � ����������� �������
		if ((millis() - CheckTimeLast) >= 1000)
		{
			CheckTimeLast = millis();
			float SecDist = (CurrSpeed * 0.278) / 1000.0; // ����������, ���������� �� �������� �������
			CurrDistance = CurrDistance + SecDist;
			// ������ ������� ������-�����-�� ����, ���� ����� ������� ����������
			CurrCalorie = CurrCalorie + SecDist * 80 * (1 + pow(sin(CurrAngle / 57.296), 0.9));
		}
	}
}

// �������� �������� ��������� �������� �� ������� ������
void CheckSpeed()
{
	const long UpdatePeriod = 300;

	if ((millis() - CheckSpeedTimeMark) > UpdatePeriod)
	{
		SendPacket('S', CurrSpeed * 10);
		CheckSpeedTimeMark = millis();
	}
}

void loop() {
	CheckButtons();
	CheckBeeper();
	CheckGercone();
	CheckPulse();
	CheckProgram();
	CheckTime();
	CheckSpeed();
	RecivePacket();
	//Serial.println((int)DisplayMode);
	PrepareDisplay();
	UpdateDisplay();
}
