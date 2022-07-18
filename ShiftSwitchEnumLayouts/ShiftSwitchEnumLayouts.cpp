// ShiftSwitchEnumLayouts.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <iostream>

int main()
{
    std::cout << "Hello World!\n";

    auto iCount = GetKeyboardLayoutList(0, nullptr);
    if (iCount == 0) {
        std::cout << "No keyboard layouts found";
        return 0;
    }

    auto hKLCurrent = GetKeyboardLayout(0);

    HKL* pHKL = new HKL[iCount];
    GetKeyboardLayoutList(iCount, pHKL);
    CHAR buffer[KL_NAMELENGTH];
    for (size_t i = 0; i < iCount; i++)
    {
        ActivateKeyboardLayout(pHKL[i], 0);
        
        if (GetKeyboardLayoutNameA(buffer)) {
            std::cout << pHKL[i] << ":" << buffer << std::endl;
        }
    }
    delete[] pHKL;

    ActivateKeyboardLayout(hKLCurrent,0);

}
