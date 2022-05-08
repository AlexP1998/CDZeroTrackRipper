// Written by Michel Helms (MichelHelms@Web.de).
// Parts of code were taken from Idael Cardoso (http://www.codeproject.com/csharp/csharpripper.asp)
//   and from Larry Osterman (http://blogs.msdn.com/larryosterman/archive/2005/05.aspx).
// Finished at 26th of September in 2006
// Of course you are allowed to cut this lines off ;)




#include "CAudioCD.h"
#include "AudioCD_Helpers.h"




// Constructor / Destructor
CAudioCD::CAudioCD( char Drive )
{
	m_hCD = NULL;
	if ( Drive != '\0' )
		Open( Drive );
}


CAudioCD::~CAudioCD()
{
	Close();
}




// Open / Close access
BOOL CAudioCD::Open( char Drive )
{
	Close();

	// Open drive-handle
	char Fn[8] = { '\\', '\\', '.', '\\', Drive, ':', '\0' };
	if ( INVALID_HANDLE_VALUE == ( m_hCD = CreateFile( Fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL ) ) )
	{
		m_hCD = NULL;
		return FALSE;
	}

	// Lock drive
	if ( ! LockCD() )
	{
		UnlockCD();
		CloseHandle( m_hCD );
		m_hCD = NULL;
		return FALSE;
	}

	// Get track-table and add it to the intern array
	ULONG BytesRead;
	CDROM_TOC Table;
	if ( 0 == DeviceIoControl( m_hCD, IOCTL_CDROM_READ_TOC, NULL, 0, &Table, sizeof(Table), &BytesRead, NULL ) )
	{
		UnlockCD();
		CloseHandle( m_hCD );
		m_hCD = NULL;
		return FALSE;
	}
	//I may be able to check for Track 0 here. After running some tests and calculations I determined that the first 804B are reserved for TOC data. I can use this information to find the first sector. 
	//Just in case I'll use sizeof(Table) instead of hard-coding the byte value into the program. According to the Red Book standard, a CD sector is 2048B long.
	for ( ULONG i=Table.FirstTrack-1; i<Table.LastTrack; i++ )
	{
		CDTRACK NewTrack;
		bool TrackZero = false;
		//Maybe I could add an "if" statement here to check if the track is 0, and if it is, check if a zero track exists. If it does, set TrackZero to true and add it to the TOC. If not, skip.
		if (i == 0)
		{
			NewTrack.Address = 0; //Start at the very beginning of the CD
			NewTrack.Length = AddressToSectors(Table.TrackData[i].Address) - NewTrack.Address;
			if (NewTrack.Length > 40)
			{
				TrackZero = true;
				TrackZeroExists = true;
			}
		}
		//If track is 0, the track to see if track 0 exists will be here. It'll check to see how long the pregap (the sectors between the ToC and Track 1) is. If it's less than maybe four sectors, track 0 does not exist.
		//If it's longer, we can assume there is a track 0. Set NewTrack.Address to the sector after the ToC and set the length to the beginning of Track 1.
		//Make this as "if track does not equal zero" or something. Also make a bool varible that denotes the existance of a track 0 and have it run if set to false. Since this is for doing the zero track, I may just have it return false or something
		if (TrackZero == false)
		{
			NewTrack.Address = AddressToSectors(Table.TrackData[i].Address);
			NewTrack.Length = AddressToSectors(Table.TrackData[i + 1].Address) - NewTrack.Address;
		}
		m_aTracks.push_back( NewTrack );
		//Why even waste CPU cycles getting the other tracks? Maybe later I'll just set this to run once.
	}


	// Return if track-count > 0
	return m_aTracks.size() > 0;
}


BOOL CAudioCD::IsOpened()
{
	return m_hCD != NULL;
}


void CAudioCD::Close()
{
	UnlockCD();
	m_aTracks.clear();
	CloseHandle( m_hCD );
	m_hCD = NULL;
}




// Read / Get track-data
ULONG CAudioCD::GetTrackCount()
{
	if ( m_hCD == NULL )
		return 0xFFFFFFFF;
	return m_aTracks.size();
}


ULONG CAudioCD::GetTrackTime( ULONG Track )
{
	if ( m_hCD == NULL )
		return 0xFFFFFFFF;
	if ( Track >= m_aTracks.size() )
		return 0xFFFFFFFF;

	CDTRACK& Tr = m_aTracks.at(Track);
	return Tr.Length / 75;
}


ULONG CAudioCD::GetTrackSize( ULONG Track )
{
	if ( m_hCD == NULL )
		return 0xFFFFFFFF;
	if ( Track >= m_aTracks.size() )
		return 0xFFFFFFFF;

	CDTRACK& Tr = m_aTracks.at(Track);
	return Tr.Length * RAW_SECTOR_SIZE;
}


BOOL CAudioCD::ReadTrack( ULONG TrackNr, CBuf<char>* pBuf )
{
	if ( m_hCD == NULL )
		return FALSE;

	if ( TrackNr >= m_aTracks.size() )
		return FALSE;
	CDTRACK& Track = m_aTracks.at(TrackNr);

	pBuf->Alloc( Track.Length*RAW_SECTOR_SIZE );

	RAW_READ_INFO Info;
	Info.TrackMode = CDDA;
	Info.SectorCount = SECTORS_AT_READ;

	ULONG i;

	for ( i=0; i<Track.Length/SECTORS_AT_READ; i++ )
	{
		Info.DiskOffset.QuadPart = (Track.Address + i*SECTORS_AT_READ) * CD_SECTOR_SIZE;
		ULONG Dummy;
		if ( 0 == DeviceIoControl( m_hCD, IOCTL_CDROM_RAW_READ, &Info, sizeof(Info), pBuf->Ptr()+i*SECTORS_AT_READ*RAW_SECTOR_SIZE, SECTORS_AT_READ*RAW_SECTOR_SIZE, &Dummy, NULL ) )
		{
			pBuf->Free();
			return FALSE;
		}
	}

	Info.SectorCount = Track.Length % SECTORS_AT_READ;
	Info.DiskOffset.QuadPart = (Track.Address + i*SECTORS_AT_READ) * CD_SECTOR_SIZE;
	ULONG Dummy;
	if ( 0 == DeviceIoControl( m_hCD, IOCTL_CDROM_RAW_READ, &Info, sizeof(Info), pBuf->Ptr()+i*SECTORS_AT_READ*RAW_SECTOR_SIZE, SECTORS_AT_READ*RAW_SECTOR_SIZE, &Dummy, NULL ) )
	{
		pBuf->Free();
		return FALSE;
	}

	return TRUE;
}


BOOL CAudioCD::ExtractTrack( ULONG TrackNr, LPCTSTR Path )
{
	if ( m_hCD == NULL )
		return FALSE;

	ULONG Dummy;

	if ( TrackNr >= m_aTracks.size() )
		return FALSE;
	CDTRACK& Track = m_aTracks.at(TrackNr);

	HANDLE hFile = CreateFile( Path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile == INVALID_HANDLE_VALUE )
		return FALSE;

	CWaveFileHeader WaveFileHeader( 44100, 16, 2, Track.Length*RAW_SECTOR_SIZE );
	WriteFile( hFile, &WaveFileHeader, sizeof(WaveFileHeader), &Dummy, NULL );

	CBuf<char> Buf( SECTORS_AT_READ * RAW_SECTOR_SIZE );

	RAW_READ_INFO Info;
	Info.TrackMode = CDDA;
	Info.SectorCount = SECTORS_AT_READ;

	ULONG i;

	for ( i=0; i<Track.Length/SECTORS_AT_READ; i++ )
	{
		Info.DiskOffset.QuadPart = (Track.Address + i*SECTORS_AT_READ) * CD_SECTOR_SIZE;
		if (0 == DeviceIoControl(m_hCD, IOCTL_CDROM_RAW_READ, &Info, sizeof(Info), Buf, SECTORS_AT_READ * RAW_SECTOR_SIZE, &Dummy, NULL))
		{
			if (TrackNr != 0)
				return FALSE;
		}

		WriteFile( hFile, Buf, Buf.Size(), &Dummy, NULL );
	}

	Info.SectorCount = Track.Length % SECTORS_AT_READ;
	Info.DiskOffset.QuadPart = (Track.Address + i*SECTORS_AT_READ) * CD_SECTOR_SIZE;
	if ( 0 == DeviceIoControl( m_hCD, IOCTL_CDROM_RAW_READ, &Info, sizeof(Info), Buf, Info.SectorCount*RAW_SECTOR_SIZE, &Dummy, NULL ) )
		return FALSE;

	WriteFile( hFile, Buf, Info.SectorCount*RAW_SECTOR_SIZE, &Dummy, NULL );

	return CloseHandle( hFile );
}




// Lock / Unlock CD-Rom Drive
BOOL CAudioCD::LockCD()
{
	if ( m_hCD == NULL )
		return FALSE;
	ULONG Dummy;
	PREVENT_MEDIA_REMOVAL pmr = { TRUE };
	return 0 != DeviceIoControl( m_hCD, IOCTL_STORAGE_MEDIA_REMOVAL, &pmr, sizeof(pmr), NULL, 0, &Dummy, NULL );
}


BOOL CAudioCD::UnlockCD()
{
	if ( m_hCD == NULL )
		return FALSE;
	ULONG Dummy;
	PREVENT_MEDIA_REMOVAL pmr = { FALSE };
	return 0 != DeviceIoControl( m_hCD, IOCTL_STORAGE_MEDIA_REMOVAL, &pmr, sizeof(pmr), NULL, 0, &Dummy, NULL );
}




// General operations
BOOL CAudioCD::InjectCD()
{
	if ( m_hCD == NULL )
		return FALSE;
	ULONG Dummy;
	return 0 != DeviceIoControl( m_hCD, IOCTL_STORAGE_LOAD_MEDIA, NULL, 0, NULL, 0, &Dummy, NULL );
}


BOOL CAudioCD::IsCDReady( char Drive )
{
	HANDLE hDrive;
	if ( Drive != '\0' )
	{
		// Open drive-handle if a drive is specified
		char Fn[8] = { '\\', '\\', '.', '\\', Drive, ':', '\0' };
		if ( INVALID_HANDLE_VALUE == ( hDrive = CreateFile( Fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL ) ) )
			return FALSE;
	}
	else
	{
		// Otherwise, take our open handle
		if ( m_hCD == NULL )
			return FALSE;
		hDrive = m_hCD;
	}

	ULONG Dummy;
	BOOL Success = DeviceIoControl( hDrive, IOCTL_STORAGE_CHECK_VERIFY2, NULL, 0, NULL, 0, &Dummy, NULL );

	if ( m_hCD != hDrive )
		CloseHandle( hDrive );

	return Success;
}


BOOL CAudioCD::EjectCD()
{
	if ( m_hCD == NULL )
		return FALSE;
	ULONG Dummy;
	return 0 != DeviceIoControl( m_hCD, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &Dummy, NULL );
}