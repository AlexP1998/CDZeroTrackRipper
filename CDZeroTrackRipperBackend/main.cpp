#include <iostream>
#include "CAudioCD.h"
#include "AudioCD_Helpers.h"

extern "C" {
    __declspec(dllexport) int RipCD(char driveDirectory, LPCSTR saveDirectory)
    {
        CAudioCD CD(driveDirectory); //Reads the table of contents on the CD.
        int errorCode = GetLastError();
        if (errorCode == 0)
        {
            if (CD.TrackZeroExists == false)
            {
                return -1;
            }
            else
            {
                if (CD.ExtractTrack(0, saveDirectory) == false)
                {
                    CD.UnlockCD();
                    return -2; //This is returning an error when it's not supposed to. Apparently the track length is too high. It stops at Sector 220
                }
                else
                {
                    return 0;
                }
            }
        }
        else
        {
            return errorCode;
        }
    }
}

