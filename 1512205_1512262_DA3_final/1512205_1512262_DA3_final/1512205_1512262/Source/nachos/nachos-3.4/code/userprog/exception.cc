// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "directory.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

/*increase PC*/
void IncreasePC() {
    int pcAfter = machine->ReadRegister(NextPCReg) + 4;
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
    machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
    machine->WriteRegister(NextPCReg, pcAfter);
}
/*add syscall handlers*/
void ExceptionHandler_Halt() {
    DEBUG('a', "Shutdown, initiated by user program.\n");
    interrupt->Halt();
}
void ExceptionHandler_ReadInt() {
    /*int: [-2147483648 , 2147483647] --> max length = 11*/
    const int maxlen = 11;
    char num_string[maxlen] = {0};
    long long ret = 0;
    for (int i = 0; i < maxlen; i++) {
        char c = 0;
        gSynchConsole->Read(&c,1);
        if (c >= '0' && c <= '9') num_string[i] = c;
        else if (i == 0 && c == '-') num_string[i] = c;
        else break;
    }
    int i = (num_string[0] == '-') ? 1 : 0;
    while (i < maxlen && num_string[i] >= '0' && num_string[i] <= '9')
        ret = ret*10 + num_string[i++] - '0';
    ret = (num_string[0] == '-') ? (-ret) : ret;
    machine->WriteRegister(2, (int)ret);
}
void ExceptionHandler_PrintInt() {
    int n = machine->ReadRegister(4);
    /*int: [-2147483648 , 2147483647] --> max length = 11*/
    const int maxlen = 11;
    char num_string[maxlen] = {0};
    int tmp[maxlen] = {0}, i = 0, j = 0;
    if (n < 0) {
        n = -n;
        num_string[i++] = '-';
    }
    do {
        tmp[j++] = n%10;
        n /= 10;
    } while (n);
    while (j) 
        num_string[i++] = '0' + (char)tmp[--j];
    gSynchConsole->Write(num_string,i);
    machine->WriteRegister(2, 0);
}
void ExceptionHandler_ReadChar() {
    char c = 0;
    gSynchConsole->Read(&c,1);
    machine->WriteRegister(2, (int)c);
}
void ExceptionHandler_PrintChar() {
    char c = (char)machine->ReadRegister(4);
    gSynchConsole->Write(&c,1);
    machine->WriteRegister(2, 0);
}
void ExceptionHandler_ReadString() {
    int buffer = machine->ReadRegister(4);
    int length = machine->ReadRegister(5);
    char *buf = NULL;
    if (length > 0) {
        buf = new char[length];
        if (buf == NULL) {
            char msg[] = "Not enough memory in system.\n";
            gSynchConsole->Write(msg,strlen(msg));
        }
        else
            memset(buf, 0, length);
    }
    if (buf != NULL) {
        /*make sure string is null terminated*/
        gSynchConsole->Read(buf,length-1);
        int n = strlen(buf)+1;
        for (int i = 0; i < n; i++) {
            machine->WriteMem(buffer + i, 1, (int)buf[i]);
        }
        delete[] buf;
    }	    
    machine->WriteRegister(2, 0);
}
void ExceptionHandler_PrintString() {
    int buffer = machine->ReadRegister(4), i = 0;
    /*limit the length of strings to print both null and non-null terminated strings*/
    const int maxlen = 256;
    char s[maxlen] = {0};
    while (i < maxlen) {
        int oneChar = 0;
        machine->ReadMem(buffer+i, 1, &oneChar);
        if (oneChar == 0) break;
        s[i++] = (char)oneChar;
    }
    gSynchConsole->Write(s,i);
    machine->WriteRegister(2, 0);
}
void ExceptionHandler_CreateF() {
    /*load file name from user to kernel*/
    int name = machine->ReadRegister(4);
    char s[FileNameMaxLen+1] = {0};
    for (int i = 0; i < FileNameMaxLen; ++i) {
        int oneChar = 0;
        machine->ReadMem(name+i, 1, &oneChar);
        if (oneChar == 0) break;
        s[i] = (char)oneChar;
    }
    /*create new file*/
    bool chk = fileSystem->Create(s, 0);
    if (chk) machine->WriteRegister(2, 0);
    else machine->WriteRegister(2, -1);
}
void ExceptionHandler_OpenF() {
    /*load file name from user to kernel*/
    int name = machine->ReadRegister(4);
    char s[FileNameMaxLen+1] = {0};
    for (int j = 0; j < FileNameMaxLen; ++j) {
        int oneChar = 0;
        machine->ReadMem(name+j, 1, &oneChar);
        if (oneChar == 0) break;
        s[j] = (char)oneChar;
    }
    /*open file*/
    OpenFile* pFile = fileSystem->Open(s);
    if (pFile == NULL) {
        machine->WriteRegister(2, -1);
        return;
    }
    /*load file type from user to kernel*/
    int type = machine->ReadRegister(5);
    /*add open file to file table*/
    int ret = gFTable->Open(pFile, type);
    if (ret == -1) {
        delete pFile;
        machine->WriteRegister(2, -1);
        return;
    }
    else {
        machine->WriteRegister(2, ret+2);
        return;
    }
}
void ExceptionHandler_CloseF() {
    /*load fid from user to kernel*/
    int fid = machine->ReadRegister(4);
    /*fid: console input & output*/
    if (fid == ConsoleInput || fid == ConsoleOutput) {
        machine->WriteRegister(2, -1);
        return;
    }
    /*fid: not console input & output*/
    fid -= 2;
    int ret = gFTable->Close(fid);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_ReadF() {
    int buffer = machine->ReadRegister(4);
    int charcount = machine->ReadRegister(5);
    int fid = machine->ReadRegister(6);
    if (charcount < 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    int i = 0;
    if (fid == ConsoleInput) {
        /*read from console input*/            
        while (i < charcount) {
            char oneChar = 0;
            int ret = gSynchConsole->Read(&oneChar,1);
            if (ret == -1) {
                machine->WriteRegister(2, -2);
                return;
            } else if (ret == 0) break; 
            machine->WriteMem(buffer + i, 1, (int)oneChar);
            ++i;
        }
        machine->WriteRegister(2, i);
        return;      
    }
    /*read from file*/
    fid -= 2;
    if (gFTable->getType(fid) == -1) {
        machine->WriteRegister(2, -1);
        return;
    }
    while (i < charcount) {
        char oneChar = 0;
        if (gFTable->ReadChar(oneChar, fid) == 0) break;
        machine->WriteMem(buffer + i, 1, (int)oneChar);
        ++i;
    }
    machine->WriteRegister(2, i);
}
void ExceptionHandler_WriteF() {
    int buffer = machine->ReadRegister(4);
    int charcount = machine->ReadRegister(5);
    int fid = machine->ReadRegister(6);
    if (charcount < 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    int i = 0;
    if (fid == ConsoleOutput) {            
        /*write to console output*/
        while (i < charcount) {
            int oneChar = 0;
            bool ret = machine->ReadMem(buffer + i, 1, &oneChar); 
            if (!ret) {
                machine->WriteRegister(2, -1);
                return;
            }
            char c = (char)oneChar;
            gSynchConsole->Write(&c,1);
            ++i;
        }
        machine->WriteRegister(2, i);
        return;      
    }
    /*write to file*/
    fid -= 2;
    if (gFTable->getType(fid) != 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    while (i < charcount) {
        int oneChar = 0;
        bool ret = machine->ReadMem(buffer + i, 1, &oneChar); 
        if (!ret) {
            machine->WriteRegister(2, -1);
            return;
        }
        char c = (char)oneChar;
        gFTable->WriteChar(c, fid);
        ++i;
    }
    machine->WriteRegister(2, i);
}
void ExceptionHandler_SeekF() {
    int pos = machine->ReadRegister(4);
    int fid = machine->ReadRegister(5)-2;
    /*seek file*/
    int ret = gFTable->Seek(pos, fid);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_ExecProc() {
    int name = machine->ReadRegister(4);
    if (name == 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    /*load file name from user to kernel*/
    char s[FileNameMaxLen+1] = {0};
    for (int j = 0; j < FileNameMaxLen; ++j) {
        int oneChar = 0;
        if (machine->ReadMem(name+j, 1, &oneChar) == FALSE) {
            machine->WriteRegister(2, -1);
            return;
        }
        if (oneChar == 0) break;
        s[j] = (char)oneChar;
    }
    /*execute process*/
    int ret = pTab->ExecUpdate(s);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_JoinProc() {  
    int id = machine->ReadRegister(4);
    /*join process*/
    int ret = pTab->JoinUpdate(id);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_ExitProc() { 
    int exitCode = machine->ReadRegister(4);
    /*exit process*/
    int ret = pTab->ExitUpdate(exitCode);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_CreateSemaphore() {
    int name = machine->ReadRegister(4);
    int semval = machine->ReadRegister(5);
    if (name == 0 || semval < 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    /*use SEM_MAXNAMESIZE to get name*/
    char s[SEM_MAXNAMESIZE] = {0};
    for (int i = 0; i < SEM_MAXNAMESIZE-1; ++i) {
        int oneChar = 0;
        machine->ReadMem(name+i, 1, &oneChar);
        if (oneChar == 0) break;
        s[i] = (char)oneChar;
    }
    /*create semaphore*/
    int ret = semTab->Create(s, semval);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_Wait() {
    int name = machine->ReadRegister(4);
    if (name == 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    /*use SEM_MAXNAMESIZE to get name*/
    char s[SEM_MAXNAMESIZE] = {0};
    for (int i = 0; i < SEM_MAXNAMESIZE-1; ++i) {
        int oneChar = 0;
        machine->ReadMem(name+i, 1, &oneChar);
        if (oneChar == 0) break;
        s[i] = (char)oneChar;
    }
    /*wait semaphore*/
    int ret = semTab->Wait(s);
    machine->WriteRegister(2, ret);
}
void ExceptionHandler_Signal() {
    int name = machine->ReadRegister(4);
    if (name == 0) {
        machine->WriteRegister(2, -1);
        return;
    }
    /*use SEM_MAXNAMESIZE to get name*/
    char s[SEM_MAXNAMESIZE] = {0};
    for (int i = 0; i < SEM_MAXNAMESIZE-1; ++i) {
        int oneChar = 0;
        machine->ReadMem(name+i, 1, &oneChar);
        if (oneChar == 0) break;
        s[i] = (char)oneChar;
    }
    /*signal semaphore*/
    int ret = semTab->Signal(s);
    machine->WriteRegister(2, ret);
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    /*process exceptions*/
    switch(which) {
        /*do nothing*/
        case NoException:
            break;
        /*syscall exception*/
        case SyscallException:
            switch(type) {
                case SC_Halt:
                    ExceptionHandler_Halt();
                    break;
                case SC_ReadInt:
                    ExceptionHandler_ReadInt();
                    IncreasePC();
                    break;
                case SC_PrintInt:
                    ExceptionHandler_PrintInt();
                    IncreasePC();
                    break;
                case SC_ReadChar:
                    ExceptionHandler_ReadChar();
                    IncreasePC();
                    break;
                case SC_PrintChar:
                    ExceptionHandler_PrintChar();
                    IncreasePC();
                    break;
                case SC_ReadString:
                    ExceptionHandler_ReadString();
                    IncreasePC();
                    break;
                case SC_PrintString:
                    ExceptionHandler_PrintString();
                    IncreasePC();
                    break;
                case SC_CreateF:
                    ExceptionHandler_CreateF();
                    IncreasePC();
                    break;
                case SC_OpenF:
                    ExceptionHandler_OpenF();
                    IncreasePC();
                    break;
                case SC_CloseF:
                    ExceptionHandler_CloseF();
                    IncreasePC();
                    break;
                case SC_ReadF:
                    ExceptionHandler_ReadF();
                    IncreasePC();
                    break;
                case SC_WriteF:
                    ExceptionHandler_WriteF();
                    IncreasePC();
                    break;
                case SC_SeekF:
                    ExceptionHandler_SeekF();
                    IncreasePC();
                    break;
                case SC_ExecProc:
                    ExceptionHandler_ExecProc();
                    IncreasePC();
                    break;
                case SC_JoinProc:
                    ExceptionHandler_JoinProc();
                    IncreasePC();
                    break;
                case SC_ExitProc:
                    ExceptionHandler_ExitProc();
                    break;
                case SC_CreateSemaphore:
                    ExceptionHandler_CreateSemaphore();
                    IncreasePC();
                    break;
                case SC_Wait:
                    ExceptionHandler_Wait();
                    IncreasePC();
                    break;
                case SC_Signal:
                    ExceptionHandler_Signal();
                    IncreasePC();
                    break;
                default:
                    /*other syscalls: increase pc and do nothing*/
                    IncreasePC();
            }
            break;
        /*other exceptions: print a error message and halt the machine*/
        case PageFaultException:
            printf("Unexpected user mode exception PageFaultException\n");
            ASSERT(FALSE);
            break;
        case ReadOnlyException:
            printf("Unexpected user mode exception ReadOnlyException\n");
            ASSERT(FALSE);
            break;
        case BusErrorException:
            printf("Unexpected user mode exception BusErrorException\n");
            ASSERT(FALSE);
            break;
        case AddressErrorException:
            printf("Unexpected user mode exception AddressErrorException\n");
            ASSERT(FALSE);
            break;
        case OverflowException:
            printf("Unexpected user mode exception OverflowException\n");
            ASSERT(FALSE);
            break;
        case IllegalInstrException:
            printf("Unexpected user mode exception IllegalInstrException\n");
            ASSERT(FALSE);
            break;
        case NumExceptionTypes:
            printf("Unexpected user mode exception NumExceptionTypes\n");
            ASSERT(FALSE);
            break;
        default:
            printf("Unexpected user mode exception %d %d\n", which, type);
            ASSERT(FALSE);
    }
}
