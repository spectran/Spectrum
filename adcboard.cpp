// ---------------------------------------------------------------------------

#pragma hdrstop

#include "ADCBoard.h"
#include "MainForm.h"
#include "ADCPort.h"
#include "Spectrum_thread.h"
#include <windows.h>
#include <stdio.h>
#include "usbcdd.h"

// ---------------------------------------------------------------------------

#pragma package(smart_init)

HANDLE Read;
HANDLE hEvent;
unsigned char bufwr[3] = {0};
volatile unsigned int bytes = 0;
unsigned int bytes_to_receive = 0;

unsigned char *buffer;

OVERLAPPED over = {0};
OVERLAPPED overlappedwr = {0};

extern unsigned char config_byte;
extern int avrg;
extern bool spectrum;

extern float sum;
extern float sum2;

float data1 = 0;
float fon1 = 0;
float data2 = 0;
float fon2 = 0;

float sum = 0;
float sum2 = 0;

// проверка связи
bool ADC::CheckConnection() {
	// запустить поток чтения
	bufwr[0] = 0xE5;
	bytes_to_receive = 1;
	// создать событие завершения чтения
	hEvent = CreateEvent(NULL, false, false, NULL);
	// создать приемный буффер нужного размера
	buffer = new unsigned char[bytes_to_receive];
	// запустить поток чтения
	Read = CreateThread(NULL, 0, ReadPort, NULL, 0, NULL);
	// ожидать события завершения чтения
	WaitForSingleObject(hEvent, INFINITE);
	if (buffer[0] != 0xE5) {
		delete[]buffer;
		return false;
	}
	delete[]buffer;
	return true;
}

// запись настроек в плату сбора данных
bool ADC::SetConfigs(unsigned char config, int averaging) {
	// запись байта конфигурации
	bufwr[0] = 0x3F;
	bufwr[1] = config;
	bytes_to_receive = 2;
	// создать приемный буффер нужного размера
	buffer = new unsigned char[bytes_to_receive];
	// возобновить поток
	ResumeThread(Read);
	// ожидать события завершения чтения
	WaitForSingleObject(hEvent, INFINITE);
	if (buffer[1] != config) {
		Application->MessageBoxA
			(L"Невозможно записать настройки в плату сбора данных!", L"Ошибка!",
			MB_OK | MB_SETFOREGROUND);
		delete[]buffer;
		return false;
	}
	// если все нормально, записываем число измерений
	else {
		bufwr[0] = 0x3A;
		bufwr[2] = averaging;
		averaging >>= 8;
		bufwr[1] = averaging;
		bytes_to_receive = 3;
		// создать приемный буффер нужного размера
		buffer = new unsigned char[bytes_to_receive];
		// возобновить поток
		ResumeThread(Read);
		// ожидать события завершения чтения
		WaitForSingleObject(Read, INFINITE);
		if (buffer[1] != bufwr[1]) {
			Application->MessageBoxA
				(L"Невозможно записать настройки в плату сбора данных!",
				L"Ошибка!", MB_OK | MB_SETFOREGROUND);
			delete[]buffer;
			return false;
		}
	}
	delete[]buffer;
	return true;
}

// получение данных и усреднение
bool ADC::GetDataPoint() {
	data1 = 0;
	data2 = 0;
	fon1 = 0;
	fon2 = 0;
	bufwr[0] = 0x21;
	// послать команду начала измерений
	if (config_byte == 0x01) {
		bytes_to_receive = avrg * 2;
	}
	if (config_byte == 0x02) {
		bytes_to_receive = avrg * 2;
	}
	if (config_byte == 0x03) {
		bytes_to_receive = avrg * 4;
	}
	// создать приемный буффер нужного размера
	buffer = new unsigned char[bytes_to_receive];
	// возобновить поток
	ResumeThread(Read);
	// ожидать события завершения чтения
	WaitForSingleObject(Read, INFINITE);
	// массив размера avrg*2, если один канал и avrg*4, если два канала
	if ((bytes == avrg * 2) || (bytes == avrg * 4)) {
		unsigned int dat = 0;
		unsigned char inc = 0;
		sum = 0;
		sum2 = 0;
		while (bytes > 1) {
			dat = buffer[bytes - 1];
			dat <<= 8;
			dat |= buffer[bytes - 2];
			if (inc == 0) {
				sum += dat;
				inc = 1;
			}
			else {
				sum2 += dat;
				inc = 0;
			}
			bytes -= 2;
		}
		delete[]buffer;
	}
	else {
		Application->MessageBoxA
			(L"Отсутствует синхронизация платы сбора данных!", L"Ошибка!",
			MB_OK | MB_SETFOREGROUND);
		spectrum = false;
		delete[]buffer;
		return false;
	}
	// ADC channel 1
	if (config_byte == 0x01) {
		data1 = ((sum + sum2) / avrg) * 2511 / 4095;
		// fon1 = (fn / avrg) * 2511 / 4095;
	}
	// ADC channel 2
	if (config_byte == 0x02) {
		data2 = ((sum + sum2) / avrg) * 2506 / 4095;
		// fon2 = (fn / avrg) * 2506 / 4095;
	}
	// both channels
	if (config_byte == 0x03) {
		data1 = (sum / avrg) * 2511 / 4095;
		// fon1 = (fn / avrg) * 2511 / 4095;
		data2 = (sum2 / avrg) * 2506 / 4095;
		// fon2 = (fn2 / avrg) * 2506 / 4095;
	}
	return true;
}
