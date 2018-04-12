// ---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "ADCPort.h"
#include "ADCBoard.h"
#include <iostream.h>
#include <string.h>
#pragma package(smart_init)
// ---------------------------------------------------------------------------

extern unsigned char bufwr[3];
extern HANDLE ADC_Com_Handle;
extern HANDLE Read;
extern HANDLE hEvent;
extern OVERLAPPED over;
extern OVERLAPPED overlappedwr;
extern unsigned int bytes_to_receive;
extern volatile unsigned int bytes;

extern unsigned char *buffer;

// ---------------------------------------------------------------------------
DWORD WINAPI ReadPort(LPVOID) {
	// ---- Place thread code here ----
	COMSTAT curstat = {0};
	DWORD btr, temp, mask, signal, result; // temp - переменная-заглушка
	// очистить приемный буфер COM-порта
	PurgeComm(ADC_Com_Handle, PURGE_RXCLEAR);
	// создать событие для приема; true,true - для асинхронных операций
	over.hEvent = CreateEvent(NULL, true, true, NULL);
	// создать событие
	overlappedwr.hEvent = CreateEvent(NULL, true, true, NULL);
	// маска = если принят байт
	SetCommMask(ADC_Com_Handle, EV_RXCHAR);
	while (1) {
		// послать байты
		// записать байты в порт (перекрываемая операция!)
		WriteFile(ADC_Com_Handle, (void*)bufwr, 3, &temp, &overlappedwr);
		// приостановить поток, пока не завершится перекрываемая операция WriteFile
		signal = WaitForSingleObject(overlappedwr.hEvent, INFINITE);
		// если операция завершилась успешно, установить соответствующий флажок
		if ((signal == WAIT_OBJECT_0) && (GetOverlappedResult(ADC_Com_Handle,
			&overlappedwr, &temp, true))) {
			bytes = 0;
			signal = WAIT_OBJECT_0;
			while (signal == WAIT_OBJECT_0) {
				// усыпить поток до прихода байта
				signal = WaitForSingleObject(over.hEvent, 300);
				// если событие прихода байта произошло
				if (signal == WAIT_OBJECT_0) {
					// получить количество принятых байтов
					ClearCommError(ADC_Com_Handle, &temp, &curstat);
					btr = curstat.cbInQue;
					// если все байты приняты
					if (btr == bytes_to_receive) {
						bytes = bytes_to_receive;
						// прочитать байты из порта в буфер программы
						ReadFile(ADC_Com_Handle, buffer, bytes, &temp, &over);
						// все байты приняты - выйти из цикла
						break;
					}
					else
						// ожидать приема нового байта
							result = WaitCommEvent(ADC_Com_Handle, &mask,
						&over);
				}
			}
		}
		// установить событие в сигнальное состояние
		SetEvent(hEvent);
		// приостановить поток
		SuspendThread(Read);
	}
	// перед выходом из потока закрыть объект-событие
	CloseHandle(overlappedwr.hEvent);
	CloseHandle(over.hEvent);
	// выйти из потока
	ExitThread(0);
}
// ---------------------------------------------------------------------------
