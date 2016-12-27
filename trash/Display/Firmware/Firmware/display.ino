/*
  Дисплей и панель управления беговой дорожкой.
  
  2015-12-19, Wyfinger
 */

#include <EEPROM.h>

#define Q1 13       // выходы выбора цифры
#define Q2 12
#define Q3 11
#define Q4 10
#define Q5 9
#define Q6 8
#define Q7 7
#define Q8 6

#define Buttons    A0  // кнопки (все кнопки через делитель напряжения на одной АЦП ноге)
#define BtnCount   6 

#define Gercone   A1 // геркон безопасности, 0 если замкнут
#define Beeper    A2 // пищалка со встроенным генератором

#define SeriesA  5  // декодеры
#define SeriesB  4

#define Clicker  3  // нога, на которой висит проталкиватель битов на 74HC164 (т.е. по восходящему фронту он проталкивает бит в буфер)
#define DSPReset 2  

#define PulseMeter A3 // нога на которой висит датчик пульса, в данном случае датчик показывает каждый удар сердца, т.е.
  // нужно считать частоту как с энкодера, есть еще датчики пульса, которые дают аналоговое значение от 0 до 5V, поддержку
  // таких датчиков тоже можно будет приделать потом, если ноги свободные останутся.

#define Dights   8 // разрядов в каждой серии

#define SerialSpeed   9600 // скорость обмена данными с силовым модулем


const byte Q[Dights] = {Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8};

// программы занятий, скорость*10 на минуту, записаны в виде строк, так проще
String P[] = {
  "2<FFFFF<<FFFFF<2",
  "22<<FFPPZZZPFFFPZdPF<22",
  "8",
  "",
  "",
  "",
  "",
  "",
  "",
  "000000000000000000000"
};

byte PPos = 0;  // номер минуты в программе тренировки
int CheckProgramTime = 0; //

const float MinSpeed = 1.0;
const float MaxSpeed = 12.0;

byte LineA[Dights]; // байты символов на дисплее, первая и вторая линия
byte LineB[Dights];
byte CurrDight = 0; // текущий отображаемый разряд

byte ProgramNo = 0;  // последняя использованная программа тренировки, сохраняется в EEPROM

enum {dmStop, dmProgramSelect, dmManualRun, dmProgramRun, dmError} DisplayMode = dmStop;

double CurrSpeed = 0;
int CurrTime = 0;
int CurrCalorie = 0;
double CurrDistance = 0;
int CurrPulse = 0;
long StartTime = 0;

unsigned int ButtonClickTime;  // время последнего нажатия на кнопку, для защиты от многократных нажатий
bool ButtonStatus;    // true если хоть одна кнопка нажата и false, если все отжаты
unsigned int BeeperTime;       // время включения пищалки, чтобы ограничить длительность писка

enum {ecNormal = 0, ecGercone} ErrorCode = ecNormal;

bool PulseStatus;  // последнее состояние с датчика пульса, чтобы отлавливать фронт
int PulseLastTime = 0;    // время последнего удара сердца
byte PulseAveraging[10];   // окно усреднения пульса


void setup() {
  // инициализация выходов
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
  // выключим все индикаторы
  digitalWrite(Q1, HIGH);
  digitalWrite(Q2, HIGH);
  digitalWrite(Q3, HIGH);
  digitalWrite(Q4, HIGH);
  digitalWrite(Q5, HIGH);
  digitalWrite(Q6, HIGH);
  digitalWrite(Q7, HIGH);
  digitalWrite(Q8, HIGH);
  // режим аналоговых ножек кнопок и геркона
  pinMode(Buttons, INPUT_PULLUP);
  pinMode(Gercone, INPUT_PULLUP);
  pinMode(Beeper, OUTPUT);
  // последняя используемая программа тренировки
  ProgramNo = EEPROM.read(0);
  if (ProgramNo >= sizeof(P))  ProgramNo = sizeof(P)-1;
  // подготовка дисплея
  PrepareDisplay(0, 0, 0, 0, 0);
  // связь с силовым модулем
  Serial.begin(SerialSpeed);
  // заполним окно усреднения пульса нулями
  for (int i = 0; i<sizeof(PulseAveraging); i++)
  {
    PulseAveraging[i] = 0;
  }
}

// преобразование числа в битовую маску для 7 сегментов
byte EncodeNum(byte Num)
{
  switch (Num) 
  {
    case 0 : return 0x3F;
    case 1 : return 0x06;
    case 2 : return 0x5B;
    case 3 : return 0x4F;
    case 4 : return 0x66;
    case 5 : return 0x6D;
    case 6 : return 0x7D;
    case 7 : return 0x07;
    case 8 : return 0x7F;
    case 9 : return 0x6F;
  }               
}

// добавляем точку к числу
byte AddDot(byte Num)
{
  return Num | B10000000;
}

// отобразить цифру на декодере
void ShowNum(byte NumA, byte NumB)
{
  // сбрасываем декодеры
  digitalWrite(DSPReset, LOW);
  digitalWrite(DSPReset, HIGH);
  // отсчитываем нужное значение
  for (int i = 7; i>=0; i--)
  {
    digitalWrite(SeriesA, !bitRead(NumA, i));
    digitalWrite(SeriesB, !bitRead(NumB, i));
    // кликер
    digitalWrite(Clicker, HIGH);
    digitalWrite(Clicker, LOW);
  }
}

// выделяем значение разряда, >=0 - до запятой, <0 - после запятой
byte GetDight(double Num, int Dight)
{
  return (int)(Num*pow(10,-Dight)) % 10;
}

// подготовка данных для дисплея в режиме выбора программы тренировки
void PrepareDisplayInProgramSelect()
{
  // отобразим символ 'P' и номер программы, вместо скорости,
  // а также время и дистанцию, заложенную в программу
  int ProgTime = ProgramTime(P[ProgramNo]);
  float ProgDistance = ProgramDestance(P[ProgramNo]);
  // номер программы
  LineA[0] = 0x73;   // символ 'P'
  LineA[1] = GetDight(ProgramNo+1, 1);
  if(LineA[1] > 0)  LineA[1] = EncodeNum(LineA[1]);
  LineA[2] = EncodeNum(GetDight(ProgramNo+1, 0));
  // время
  LineA[6] = EncodeNum(GetDight(ProgTime / 60, 1));
  LineA[7] = AddDot(EncodeNum(GetDight(ProgTime / 60, 0)));
  LineB[6] = AddDot(EncodeNum(GetDight(ProgTime % 60, 1)));
  LineB[7] = EncodeNum(GetDight(ProgTime % 60, 0));
  // дистанция
  if (ProgDistance < 1)  // до 1 км выводим два знака после запятой
  {
    LineA[3] = AddDot(EncodeNum(GetDight(ProgDistance, 0)));
    LineA[4] = EncodeNum(GetDight(ProgDistance, -1));
    LineA[5] = EncodeNum(GetDight(ProgDistance, -2));
  } else {           // а больше - только один
    LineA[3] = GetDight(ProgDistance, 1);
    if(LineA[3] > 0)  LineA[3] = EncodeNum(LineA[3]); 
    LineA[4] = AddDot(EncodeNum(GetDight(ProgDistance, 0)));
    LineA[5] = EncodeNum(GetDight(ProgDistance, -1));
  }
  // очищаем остальные табло
  LineB[1] = 0;
  LineB[2] = 0;
  LineB[3] = 0;
  LineB[4] = 0;
  LineB[5] = 0;
}

// подготовка данных для дисплея в режиме ошибки
void PrepareDisplayInError()
{
  // отобразим символ 'E' и номер ошибки в табло скорости
  LineA[0] = 0x79;   // символ 'E'
  LineA[1] = 0;
  LineA[2] = EncodeNum(ErrorCode);
  // очищаем остальные табло
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

// подготовка данных дисплея
void PrepareDisplay(double Speed, int Time, int Calorie, double Distance, int Pulse)
{
  if (ErrorCode > ecNormal) 
  {
    PrepareDisplayInError();
    return ;
  }
  // если дисплей в режиме выбора программы тренировки - отрисовка идет по другому
  if (DisplayMode == dmProgramSelect) 
  {
    PrepareDisplayInProgramSelect();
    return ;
  }

  // скорость
  LineA[0] = GetDight(Speed, 1);
  if(LineA[0] > 0)  LineA[0] = EncodeNum(LineA[0]);  // не отображаем ноль в начале
  LineA[1] = AddDot(EncodeNum(GetDight(Speed, 0)));
  LineA[2] = EncodeNum(GetDight(Speed, -1));
  // дистанция
  if (Distance < 1)  // до 1 км выводим два знака после запятой
  {
    LineA[3] = AddDot(EncodeNum(GetDight(Distance, 0)));
    LineA[4] = EncodeNum(GetDight(Distance, -1));
    LineA[5] = EncodeNum(GetDight(Distance, -2));
  } else {           // а больше - только один
    LineA[3] = GetDight(Distance, 1);
    if(LineA[3] > 0)  LineA[3] = EncodeNum(LineA[3]); 
    LineA[4] = AddDot(EncodeNum(GetDight(Distance, 0)));
    LineA[5] = EncodeNum(GetDight(Distance, -1));
  }
  // время, минуты
  LineA[6] = EncodeNum(GetDight(Time / 60, 1));
  LineA[7] = EncodeNum(GetDight(Time / 60, 0));
  if (Time % 2 == 0)  LineA[7] = AddDot(LineA[7]);
  // калории
  LineB[0] = GetDight(Calorie, 2);
  if(LineB[0] > 0)  LineB[0] = EncodeNum(LineB[0]);
  LineB[1] = GetDight(Calorie, 1);
  if(LineB[1] > 0)  LineB[1] = EncodeNum(LineB[1]);
  LineB[2] = EncodeNum(GetDight(Calorie, 0));
  // пульс
  LineB[3] = GetDight(Pulse, 2);
  if(LineB[3] > 0)  LineB[3] = EncodeNum(LineB[3]);
  LineB[4] = GetDight(Pulse, 1);
  if(LineB[4] > 0)  LineB[4] = EncodeNum(LineB[4]);
  LineB[5] = EncodeNum(GetDight(Pulse, 0));
  // время, секунды
  LineB[6] = EncodeNum(GetDight(Time % 60, 1)); // не больше 59, этож секунды
  LineB[7] = EncodeNum(GetDight(Time % 60, 0));  
  if (Time % 2 == 0)  LineB[6] = AddDot(LineB[6]);  
}

// подсчет времени в тренировочной программе
int ProgramTime(String Program)
{
  return  60 * Program.length();
}

// подсчет дистанции в тренировочной программе
float ProgramDestance(String Program)
{
  float Summ = 0.0001;
  for (int i = 0; i<=Program.length(); i++)
  {
    Summ = Summ + Program[i];   // км/ч, сумма по минутам
  }
  return Summ / 600.0;
}

// обновление содержимого дисплея
void UpdateDisplay()
{
  // выключаем предыдущий разряд
  digitalWrite(Q[CurrDight], HIGH);
  // проталкиваем в декодер следующий разряд
  CurrDight++;
  if (CurrDight==Dights)  CurrDight = 0;
  ShowNum(LineA[CurrDight], LineB[CurrDight]);
  // включаем новый разряд
  digitalWrite(Q[CurrDight], LOW);
}

// отправка сообщения на силовой модуль
void SendPacket(char Code, long Value) 
{
  byte size = sizeof(Code) + sizeof(Value) + sizeof(byte);
  byte buff[size];

  buff[0] = 'D';  // это означает, что пакет от дисплея в силовой модуль
  buff[1] = Code;
  buff[2] = Value & 0xFF;
  buff[3] = (Value >> 8) & 0xFF;
  buff[4] = buff[0] ^ buff[1] ^ buff[2] ^ buff[3];
  Serial.write(buff, size);
}

// отправляем на силовой модуль требуемую скорость
void SendCurrSpeed()
{
  SendPacket('S', CurrSpeed * 10);
}

// отправляем на силовой модуль команду отключить реле (при ошибке, скорость тоже будет сброшена)
void SendError()
{
  SendPacket('E', 0);
}

// пискнуть
void Beep()
{
  if (ButtonStatus==false)  // пикнуть можно только если кнопки не нажаты, т.е. только в момент нажатия,
  {                         // но не при многократных нажатиях (без отпускания кнопки)
    BeeperTime = millis();
    digitalWrite(Beeper, HIGH);
  }
} 

// выключаем пищалку
void CheckBeeper()
{
  if ((millis() - BeeperTime) > 100)
  {
    digitalWrite(Beeper, LOW);
  }
}

//** Обработка нажатий кнопок

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
    } else {
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
 if (DisplayMode == dmStop)  // если в режиме остановки - начинаем тренировку в ручном режиме с минимальной скорости
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
 if (DisplayMode == dmProgramSelect) // если в режиме выбора программы - начинаем работу по программе
 {
   DisplayMode = dmProgramRun;
   StartTime = millis();
   PPos = 0;
   CurrSpeed = (float)P[ProgramNo][PPos] / 10.0;
   Beep();
   return;
 }
 if ((DisplayMode == dmManualRun) || (DisplayMode == dmProgramRun)) // если в режиме тренировки - остановка
 {
   DisplayMode = dmStop;
   CurrSpeed = 0; // обнуляем скорость, но не остальные показатели
   Beep();
   return;
 }
}

void ButtonSelect()
{
  // если находимся в режиме выбора программы - увеличим выбранную программу
  if (DisplayMode == dmProgramSelect) 
  {
    ProgramNo++;
    if (ProgramNo >= (sizeof(P) / sizeof(P[0])) )  ProgramNo = 0;
    EEPROM.write(0, ProgramNo);
    Beep();
  }
  // перейти в режим выбора программы можно только находясь в режиме останова
  if (DisplayMode == dmStop) 
  {    
    DisplayMode = dmProgramSelect;
    Beep();
  }
}

void ButtonSet()
{
  // нафига вообще нужна кнопка Set ???
}

// контроль нажатия на кнопки клавиатуры
void CheckButtons()
{
  if (millis() - ButtonClickTime > 100)
  {
    bool NewButtonStatus = false;

    int maxVal = 1024;               // максимальное значение АЦП (10 бит)
    int stepVal = maxVal / (BtnCount); // шаг делителя напряжения
    int halfVal = stepVal / 2;       // это все должно на этапе компиляции вычисляться

    int Val = analogRead(Buttons);
    
    if ((Val > 0*stepVal-halfVal) && (Val < 0*stepVal+halfVal))
    {
      NewButtonStatus = true;
      ButtonStartStop();
    }
    if ((Val > 1*stepVal-halfVal) && (Val < 1*stepVal+halfVal))
    {
      NewButtonStatus = true;
      ButtonSet();
    }
    if ((Val > 2*stepVal-halfVal) && (Val < 2*stepVal+halfVal))
    {
      NewButtonStatus = true;
      ButtonDown();
    }
    if ((Val > 3*stepVal-halfVal) && (Val < 3*stepVal+halfVal))
    {
      NewButtonStatus = true;
      ButtonSelect();
    }
    if ((Val > 4*stepVal-halfVal) && (Val < 4*stepVal+halfVal))
    {
      NewButtonStatus = true;
      ButtonUp();
    }
    if ((Val > 5*stepVal-halfVal) && (Val < 5*stepVal+halfVal))
    {
      NewButtonStatus = true;
      ButtonReset();
    }
    // событие отжатия кнопки
    if ((NewButtonStatus == false) && (ButtonStatus == true))
    {
      //
    }
    ButtonStatus = NewButtonStatus;
    ButtonClickTime = millis();
  }
}

// если находимся в режиме работы по программе - раз в минуту переходим к следующему этапу
void CheckProgram()
{
  if (DisplayMode == dmProgramRun)
  {
    if ((CurrTime % 60 == 0) && (CheckProgramTime % 60 == 59))
    {
      PPos++;
      if (PPos >= P[ProgramNo].length()) // программа тренировки окончена
      {
        DisplayMode = dmStop;
        CurrSpeed = 0;
        PPos = 0;
        Beep();
      } else {
        CurrSpeed = (float)P[ProgramNo][PPos] / 10.0;
      }      
    }
    CheckProgramTime = CurrTime;
  }
}

// если геркон не замкнут - отображаем ошибку и отключаем мотор (и релюху тоже)
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
    SendError();  // это остановит мотор и отключит реле
  } else {
    if ((DisplayMode == dmError) && (ErrorCode > ecNormal))
    {
      DisplayMode = dmStop;  /// TODO: нужно ловить момент изменения состояния
      ErrorCode = ecNormal;
    }
  }
}

// подсчет частоты пульса
void CheckPulse()
{
  // ловим нисходящий фронт - это "удар" сердца
  bool Status = digitalRead(PulseMeter);
  if ((Status == false) && (PulseStatus == true)) // нисходящий фронт 
  {
    int PulsePeriod = millis() - PulseLastTime;  // период сердечных сокращений
    if ((PulsePeriod > 240)  && (PulsePeriod < 3000))   // левые какие-то данные, отсекаем (пульс от 20 до 250 уд/мин)
    {
      long PulseRaw = 60000 / PulsePeriod;  // измеренный пульс по времени между двумя ударами
      for (int i = sizeof(PulseAveraging)-1; i==1; i--)   // сдвигаем данные в окне усреднения
      {
        PulseAveraging[i] = PulseAveraging[i-1];
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

void loop() {
  CheckButtons();  
  CheckBeeper();
  CheckGercone();
  CheckPulse();
  CheckProgram();
  if ((DisplayMode == dmManualRun) || (DisplayMode == dmProgramRun))
  {
    CurrTime = (millis() - StartTime) / 1000;
  }
  Serial.println((int)DisplayMode);
  PrepareDisplay(CurrSpeed, CurrTime, CurrCalorie, CurrDistance, CurrPulse);
  UpdateDisplay();
}
