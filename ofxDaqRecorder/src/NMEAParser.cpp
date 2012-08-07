///////////////////////////////////////////////////////////////////////////////
// NMEAParser.cpp: 
// Desctiption:	Implementation of the CNMEAParser class.
///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1998-2002 VGPS
// All rights reserved.
//
// VGPS licenses this source code for use within your application in
// object form. This source code is not to be distributed in any way without
// prior written permission from VGPS.
//
// Visual Source Safe: $Revision: 8 $
///////////////////////////////////////////////////////////////////////////////
//#include "stdafx.h"
#include "NMEAParser.h"

#define MAXFIELD	25		// maximum field length





//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CNMEAParser::CNMEAParser()
{
	m_nState = NP_STATE_SOM;
	m_dwCommandCount = 0;
	Reset();
}

CNMEAParser::~CNMEAParser()
{
}

///////////////////////////////////////////////////////////////////////////////
// ParseBuffer:	Parse the supplied buffer for NMEA sentence information. Since
//				the parser is a state machine, partial NMEA sentence data may
//				be supplied where the next time this method is called, the
//				rest of the partial NMEA sentence will complete	the sentence.
//
//				NOTE:
//
// Returned:	TRUE all the time....
///////////////////////////////////////////////////////////////////////////////
BOOL CNMEAParser::ParseBuffer(BYTE *pBuff, int dwLen)
{
	for(int i = 0; i < dwLen; i++)
	{
		ProcessNMEA(pBuff[i]);
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// ProcessNMEA: This method is the main state machine which processes individual
//				bytes from the buffer and parses a NMEA sentence. A typical
//				sentence is constructed as:
//
//					$CMD,DDDD,DDDD,....DD*CS<CR><LF>
//
//				Where:
//						'$'			HEX 24 Start of sentence
//						'CMD'		Address/NMEA command
//						',DDDD'		Zero or more data fields
//						'*CS'		Checksum field
//						<CR><LF>	Hex 0d 0A End of sentence
//
//				When a valid sentence is received, this function sends the
//				NMEA command and data to the ProcessCommand method for
//				individual field parsing.
//
//				NOTE:
//						
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessNMEA(BYTE btData)
{
	switch(m_nState)
	{
		///////////////////////////////////////////////////////////////////////
		// Search for start of message '$'
		case NP_STATE_SOM :
			if(btData == '$')
			{
				m_btChecksum = 0;			// reset checksum
				m_wIndex = 0;				// reset index
				m_nState = NP_STATE_CMD;
			}
			
		break;

		///////////////////////////////////////////////////////////////////////
		// Retrieve command (NMEA Address)
		case NP_STATE_CMD :
			if(btData != ',' && btData != '*')
			{
				m_pCommand[m_wIndex++] = btData;
				m_btChecksum ^= btData;

				// Check for command overflow
				if(m_wIndex >= NP_MAX_CMD_LEN)
				{
					m_nState = NP_STATE_SOM;
				}
			}
			else
			{
				m_pCommand[m_wIndex] = '\0';	// terminate command
				m_btChecksum ^= btData;
				m_wIndex = 0;
				m_nState = NP_STATE_DATA;		// goto get data state
			}
		break;

		///////////////////////////////////////////////////////////////////////
		// Store data and check for end of sentence or checksum flag
		case NP_STATE_DATA :
			if(btData == '*') // checksum flag?
			{
				m_pData[m_wIndex] = '\0';
				m_nState = NP_STATE_CHECKSUM_1;
			}
			else // no checksum flag, store data
			{
				//
				// Check for end of sentence with no checksum
				//
				if(btData == '\r')
				{
					m_pData[m_wIndex] = '\0';
					ProcessCommand(m_pCommand, m_pData);
					m_nState = NP_STATE_SOM;
					return;
				}

				//
				// Store data and calculate checksum
				//
				m_btChecksum ^= btData;
				m_pData[m_wIndex] = btData;
				if(++m_wIndex >= NP_MAX_DATA_LEN) // Check for buffer overflow
				{
					m_nState = NP_STATE_SOM;
				}
			}
		break;

		///////////////////////////////////////////////////////////////////////
		case NP_STATE_CHECKSUM_1 :
			if( (btData - '0') <= 9)
			{
				m_btReceivedChecksum = (btData - '0') << 4;
			}
			else
			{
				m_btReceivedChecksum = (btData - 'A' + 10) << 4;
			}

			m_nState = NP_STATE_CHECKSUM_2;

		break;

		///////////////////////////////////////////////////////////////////////
		case NP_STATE_CHECKSUM_2 :
			if( (btData - '0') <= 9)
			{
				m_btReceivedChecksum |= (btData - '0');
			}
			else
			{
				m_btReceivedChecksum |= (btData - 'A' + 10);
			}

			if(m_btChecksum == m_btReceivedChecksum)
			{
				ProcessCommand(m_pCommand, m_pData);
			}

			m_nState = NP_STATE_SOM;
		break;

		///////////////////////////////////////////////////////////////////////
		default : m_nState = NP_STATE_SOM;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Process NMEA sentence - Use the NMEA address (*pCommand) and call the
// appropriate sentense data prossor.
///////////////////////////////////////////////////////////////////////////////
BOOL CNMEAParser::ProcessCommand(BYTE *pCommand, BYTE *pData)
{
	//
	// GPGGA
	//
	if( strcmp((char *)pCommand, "GPGGA") == NULL )
	{
		ProcessGPGGA(pData);
	}

	//
	// GPGSA
	//
	else if( strcmp((char *)pCommand, "GPGSA") == NULL )
	{
		ProcessGPGSA(pData);
	}

	//
	// GPGSV
	//
	else if( strcmp((char *)pCommand, "GPGSV") == NULL )
	{
		ProcessGPGSV(pData);
	}

	//
	// GPRMB
	//
	else if( strcmp((char *)pCommand, "GPRMB") == NULL )
	{
		ProcessGPRMB(pData);
	}

	//
	// GPRMC
	//
	else if( strcmp((char *)pCommand, "GPRMC") == NULL )
	{
		ProcessGPRMC(pData);
	}

	//
	// GPZDA
	//
	else if( strcmp((char *)pCommand, "GPZDA") == NULL )
	{
		ProcessGPZDA(pData);
	}

	m_dwCommandCount++;
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Name:		GetField
//
// Description:	This function will get the specified field in a NMEA string.
//
// Entry:		BYTE *pData -		Pointer to NMEA string
//				BYTE *pField -		pointer to returned field
//				int nfieldNum -		Field offset to get
//				int nMaxFieldLen -	Maximum of bytes pFiled can handle
///////////////////////////////////////////////////////////////////////////////
BOOL CNMEAParser::GetField(BYTE *pData, BYTE *pField, int nFieldNum, int nMaxFieldLen)
{
	//
	// Validate params
	//
	if(pData == NULL || pField == NULL || nMaxFieldLen <= 0)
	{
		return FALSE;
	}

	//
	// Go to the beginning of the selected field
	//
	int i = 0;
	int nField = 0;
	while(nField != nFieldNum && pData[i])
	{
		if(pData[i] == ',')
		{
			nField++;
		}

		i++;

		if(pData[i] == NULL)
		{
			pField[0] = '\0';
			return FALSE;
		}
	}

	if(pData[i] == ',' || pData[i] == '*')
	{
		pField[0] = '\0';
		return FALSE;
	}

	//
	// copy field from pData to Field
	//
	int i2 = 0;
	while(pData[i] != ',' && pData[i] != '*' && pData[i])
	{
		pField[i2] = pData[i];
		i2++; i++;

		//
		// check if field is too big to fit on passed parameter. If it is,
		// crop returned field to its max length.
		//
		if(i2 >= nMaxFieldLen)
		{
			i2 = nMaxFieldLen-1;
			break;
		}
	}
	pField[i2] = '\0';

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Reset: Reset all NMEA data to start-up default values.
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::Reset()
{
	int i;

	//
	// GPGGA Data
	//
	m_btGGAHour = 0;					//
	m_btGGAMinute = 0;					//
	m_btGGASecond = 0;					//
	m_dGGALatitude = 0.0;				// < 0 = South, > 0 = North 
	m_dGGALongitude = 0.0;				// < 0 = West, > 0 = East
	m_btGGAGPSQuality = 0;				// 0 = fix not available, 1 = GPS sps mode, 2 = Differential GPS, SPS mode, fix valid, 3 = GPS PPS mode, fix valid
	m_btGGANumOfSatsInUse = 0;			//
	m_dGGAHDOP = 0.0;					//
	m_dGGAAltitude = 0.0;				// Altitude: mean-sea-level (geoid) meters
	m_dwGGACount = 0;					//
	m_nGGAOldVSpeedSeconds = 0;			//
	m_dGGAOldVSpeedAlt = 0.0;			//
	m_dGGAVertSpeed = 0.0;				//

	//
	// GPGSA
	//
	m_btGSAMode = 'M';					// M = manual, A = automatic 2D/3D
	m_btGSAFixMode = 1;					// 1 = fix not available, 2 = 2D, 3 = 3D
	for(i = 0; i < NP_MAX_CHAN; i++)
	{
		m_wGSASatsInSolution[i] = 0;	// ID of sats in solution
	}
	m_dGSAPDOP = 0.0;					//
	m_dGSAHDOP = 0.0;					//
	m_dGSAVDOP = 0.0;					//
	m_dwGSACount = 0;					//

	//
	// GPGSV
	//
	m_btGSVTotalNumOfMsg = 0;			//
	m_wGSVTotalNumSatsInView = 0;		//
	for(i = 0; i < NP_MAX_CHAN; i++)
	{
		m_GSVSatInfo[i].m_wAzimuth = 0;
		m_GSVSatInfo[i].m_wElevation = 0;
		m_GSVSatInfo[i].m_wPRN = 0;
		m_GSVSatInfo[i].m_wSignalQuality = 0;
		m_GSVSatInfo[i].m_bUsedInSolution = FALSE;
	}
	m_dwGSVCount = 0;

	//
	// GPRMB
	//
	m_btRMBDataStatus = 'V';			// A = data valid, V = navigation receiver warning
	m_dRMBCrosstrackError = 0.0;		// nautical miles
	m_btRMBDirectionToSteer = '?';		// L/R
	m_lpszRMBOriginWaypoint[0] = '\0';	// Origin Waypoint ID
	m_lpszRMBDestWaypoint[0] = '\0';	// Destination waypoint ID
	m_dRMBDestLatitude = 0.0;			// destination waypoint latitude
	m_dRMBDestLongitude = 0.0;			// destination waypoint longitude
	m_dRMBRangeToDest = 0.0;			// Range to destination nautical mi
	m_dRMBBearingToDest = 0.0;			// Bearing to destination, degrees true
	m_dRMBDestClosingVelocity = 0.0;	// Destination closing velocity, knots
	m_btRMBArrivalStatus = 'V';			// A = arrival circle entered, V = not entered
	m_dwRMBCount = 0;					//

	//
	// GPRMC
	//
	m_btRMCHour = 0;					//
	m_btRMCMinute = 0;					//
	m_btRMCSecond = 0;					//
	m_btRMCDataValid = 'V';				// A = Data valid, V = navigation rx warning
	m_dRMCLatitude = 0.0;				// current latitude
	m_dRMCLongitude = 0.0;				// current longitude
	m_dRMCGroundSpeed = 0.0;			// speed over ground, knots
	m_dRMCCourse = 0.0;					// course over ground, degrees true
	m_btRMCDay = 1;						//
	m_btRMCMonth = 1;					//
	m_wRMCYear = 2000;					//
	m_dRMCMagVar = 0.0;					// magnitic variation, degrees East(+)/West(-)
	m_dwRMCCount = 0;					//

	//
	// GPZDA
	//
	m_btZDAHour = 0;					//
	m_btZDAMinute = 0;					//
	m_btZDASecond = 0;					//
	m_btZDADay = 1;						// 1 - 31
	m_btZDAMonth = 1;					// 1 - 12
	m_wZDAYear = 2000;					//
	m_btZDALocalZoneHour = 0;			// 0 to +/- 13
	m_btZDALocalZoneMinute = 0;			// 0 - 59
	m_dwZDACount = 0;					//
}

///////////////////////////////////////////////////////////////////////////////
// Check to see if supplied satellite ID is used in the GPS solution.
// Retruned:	BOOL -	TRUE if satellate ID is used in solution
//						FALSE if not used in solution.
///////////////////////////////////////////////////////////////////////////////
BOOL CNMEAParser::IsSatUsedInSolution(WORD wSatID)
{
	if(wSatID == 0) return FALSE;
	for(int i = 0; i < 12; i++)
	{
		if(wSatID == m_wGSASatsInSolution[i])
		{
			return TRUE;
		}
	}

	return FALSE;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessGPGGA(BYTE *pData)
{
	BYTE pField[MAXFIELD];
	CHAR pBuff[10];

	//
	// Time
	//
	if(GetField(pData, pField, 0, MAXFIELD))
	{
		// Hour
		pBuff[0] = pField[0];
		pBuff[1] = pField[1];
		pBuff[2] = '\0';
		m_btGGAHour = atoi(pBuff);

		// minute
		pBuff[0] = pField[2];
		pBuff[1] = pField[3];
		pBuff[2] = '\0';
		m_btGGAMinute = atoi(pBuff);

		// Second
		pBuff[0] = pField[4];
		pBuff[1] = pField[5];
		pBuff[2] = '\0';
		m_btGGASecond = atoi(pBuff);
	}

	//
	// Latitude
	//
	if(GetField(pData, pField, 1, MAXFIELD))
	{
		m_dGGALatitude = atof((CHAR *)pField+2) / 60.0;
		pField[2] = '\0';
		m_dGGALatitude += atof((CHAR *)pField);

	}
	if(GetField(pData, pField, 2, MAXFIELD))
	{
		if(pField[0] == 'S')
		{
			m_dGGALatitude = -m_dGGALatitude;
		}
	}

	//
	// Longitude
	//
	if(GetField(pData, pField, 3, MAXFIELD))
	{
		m_dGGALongitude = atof((CHAR *)pField+3) / 60.0;
		pField[3] = '\0';
		m_dGGALongitude += atof((CHAR *)pField);
	}
	if(GetField(pData, pField, 4, MAXFIELD))
	{
		if(pField[0] == 'W')
		{
			m_dGGALongitude = -m_dGGALongitude;
		}
	}

	//
	// GPS quality
	//
	if(GetField(pData, pField, 5, MAXFIELD))
	{
		m_btGGAGPSQuality = pField[0] - '0';
	}

	//
	// Satellites in use
	//
	if(GetField(pData, pField, 6, MAXFIELD))
	{
		pBuff[0] = pField[0];
		pBuff[1] = pField[1];
		pBuff[2] = '\0';
		m_btGGANumOfSatsInUse = atoi(pBuff);
	}

	//
	// HDOP
	//
	if(GetField(pData, pField, 7, MAXFIELD))
	{
		m_dGGAHDOP = atof((CHAR *)pField);
	}
	
	//
	// Altitude
	//
	if(GetField(pData, pField, 8, MAXFIELD))
	{
		m_dGGAAltitude = atof((CHAR *)pField);
	}

	//
	// Durive vertical speed (bonus)
	//
	int nSeconds = (int)m_btGGAMinute * 60 + (int)m_btGGASecond;
	if(nSeconds > m_nGGAOldVSpeedSeconds)
	{
		double dDiff = (double)(m_nGGAOldVSpeedSeconds-nSeconds);
		double dVal = dDiff/60.0;
		if(dVal != 0.0)
		{
			m_dGGAVertSpeed = (m_dGGAOldVSpeedAlt - m_dGGAAltitude) / dVal;
		}
	}
	m_dGGAOldVSpeedAlt = m_dGGAAltitude;
	m_nGGAOldVSpeedSeconds = nSeconds;

	m_dwGGACount++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessGPGSA(BYTE *pData)
{
	BYTE pField[MAXFIELD];
	CHAR pBuff[10];

	//
	// Mode
	//
	if(GetField(pData, pField, 0, MAXFIELD))
	{
		m_btGSAMode = pField[0];
	}

	//
	// Fix Mode
	//
	if(GetField(pData, pField, 1, MAXFIELD))
	{
		m_btGSAFixMode = pField[0] - '0';
	}

	//
	// Active satellites
	//
	for(int i = 0; i < 12; i++)
	{
		if(GetField(pData, pField, 2 + i, MAXFIELD))
		{
			pBuff[0] = pField[0];
			pBuff[1] = pField[1];
			pBuff[2] = '\0';
			m_wGSASatsInSolution[i] = atoi(pBuff);
		}
		else
		{
			m_wGSASatsInSolution[i] = 0;
		}
	}

	//
	// PDOP
	//
	if(GetField(pData, pField, 14, MAXFIELD))
	{
		m_dGSAPDOP = atof((CHAR *)pField);
	}
	else
	{
		m_dGSAPDOP = 0.0;
	}

	//
	// HDOP
	//
	if(GetField(pData, pField, 15, MAXFIELD))
	{
		m_dGSAHDOP = atof((CHAR *)pField);
	}
	else
	{
		m_dGSAHDOP = 0.0;
	}

	//
	// VDOP
	//
	if(GetField(pData, pField, 16, MAXFIELD))
	{
		m_dGSAVDOP = atof((CHAR *)pField);
	}
	else
	{
		m_dGSAVDOP = 0.0;
	}

	m_dwGSACount++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessGPGSV(BYTE *pData)
{
	INT nTotalNumOfMsg, nMsgNum;
	BYTE pField[MAXFIELD];

	//
	// Total number of messages
	//
	if(GetField(pData, pField, 0, MAXFIELD))
	{
		nTotalNumOfMsg = atoi((CHAR *)pField);

		//
		// Make sure that the nTotalNumOfMsg is valid. This is used to
		// calculate indexes into an array. I've seen corrept NMEA strings
		// with no checksum set this to large values.
		//
		if(nTotalNumOfMsg > 9 || nTotalNumOfMsg < 0) return; 
	}
	if(nTotalNumOfMsg < 1 || nTotalNumOfMsg*4 >= NP_MAX_CHAN)
	{
		return;
	}

	//
	// message number
	//
	if(GetField(pData, pField, 1, MAXFIELD))
	{
		nMsgNum = atoi((CHAR *)pField);

		//
		// Make sure that the message number is valid. This is used to
		// calculate indexes into an array
		//
		if(nMsgNum > 9 || nMsgNum < 0) return; 
	}

	//
	// Total satellites in view
	//
	if(GetField(pData, pField, 2, MAXFIELD))
	{
		m_wGSVTotalNumSatsInView = atoi((CHAR *)pField);
	}

	//
	// Satelite data
	//
	for(int i = 0; i < 4; i++)
	{
		// Satellite ID
		if(GetField(pData, pField, 3 + 4*i, MAXFIELD))
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wPRN = atoi((CHAR *)pField);
		}
		else
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wPRN = 0;
		}

		// Elevarion
		if(GetField(pData, pField, 4 + 4*i, MAXFIELD))
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wElevation = atoi((CHAR *)pField);
		}
		else
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wElevation = 0;
		}

		// Azimuth
		if(GetField(pData, pField, 5 + 4*i, MAXFIELD))
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wAzimuth = atoi((CHAR *)pField);
		}
		else
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wAzimuth = 0;
		}

		// SNR
		if(GetField(pData, pField, 6 + 4*i, MAXFIELD))
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wSignalQuality = atoi((CHAR *)pField);
		}
		else
		{
			m_GSVSatInfo[i+(nMsgNum-1)*4].m_wSignalQuality = 0;
		}

		//
		// Update "used in solution" (m_bUsedInSolution) flag. This is base
		// on the GSA message and is an added convenience for post processing
		//
		m_GSVSatInfo[i+(nMsgNum-1)*4].m_bUsedInSolution = IsSatUsedInSolution(m_GSVSatInfo[i+(nMsgNum-1)*4].m_wPRN);
	}

	m_dwGSVCount++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessGPRMB(BYTE *pData)
{
	BYTE pField[MAXFIELD];

	//
	// Data status
	//
	if(GetField(pData, pField, 0, MAXFIELD))
	{
		m_btRMBDataStatus = pField[0];
	}
	else
	{
		m_btRMBDataStatus = 'V';
	}

	//
	// Cross track error
	//
	if(GetField(pData, pField, 1, MAXFIELD))
	{
		m_dRMBCrosstrackError = atof((CHAR *)pField);
	}
	else
	{
		m_dRMBCrosstrackError = 0.0;
	}

	//
	// Direction to steer
	//
	if(GetField(pData, pField, 2, MAXFIELD))
	{
		m_btRMBDirectionToSteer = pField[0];
	}
	else
	{
		m_btRMBDirectionToSteer = '?';
	}

	//
	// Orgin waypoint ID
	//
	if(GetField(pData, pField, 3, MAXFIELD))
	{
		strcpy(m_lpszRMBOriginWaypoint, (CHAR *)pField);
	}
	else
	{
		m_lpszRMBOriginWaypoint[0] = '\0';
	}

	//
	// Destination waypoint ID
	//
	if(GetField(pData, pField, 4, MAXFIELD))
	{
		strcpy(m_lpszRMBDestWaypoint, (CHAR *)pField);
	}
	else
	{
		m_lpszRMBDestWaypoint[0] = '\0';
	}

	//
	// Destination latitude
	//
	if(GetField(pData, pField, 5, MAXFIELD))
	{
		m_dRMBDestLatitude = atof((CHAR *)pField+2) / 60.0;
		pField[2] = '\0';
		m_dRMBDestLatitude += atof((CHAR *)pField);

	}
	if(GetField(pData, pField, 6, MAXFIELD))
	{
		if(pField[0] == 'S')
		{
			m_dRMBDestLatitude = -m_dRMBDestLatitude;
		}
	}

	//
	// Destination Longitude
	//
	if(GetField(pData, pField, 7, MAXFIELD))
	{
		m_dRMBDestLongitude = atof((CHAR *)pField+3) / 60.0;
		pField[3] = '\0';
		m_dRMBDestLongitude += atof((CHAR *)pField);
	}
	if(GetField(pData, pField, 8, MAXFIELD))
	{
		if(pField[0] == 'W')
		{
			m_dRMBDestLongitude = -m_dRMBDestLongitude;
		}
	}

	//
	// Range to destination nautical mi
	//
	if(GetField(pData, pField, 9, MAXFIELD))
	{
		m_dRMBRangeToDest = atof((CHAR *)pField);
	}
	else
	{
		m_dRMBCrosstrackError = 0.0;
	}

	//
	// Bearing to destination degrees true
	//
	if(GetField(pData, pField, 10, MAXFIELD))
	{
		m_dRMBBearingToDest = atof((CHAR *)pField);
	}
	else
	{
		m_dRMBBearingToDest = 0.0;
	}

	//
	// Closing velocity
	//
	if(GetField(pData, pField, 11, MAXFIELD))
	{
		m_dRMBDestClosingVelocity = atof((CHAR *)pField);
	}
	else
	{
		m_dRMBDestClosingVelocity = 0.0;
	}

	//
	// Arrival status
	//
	if(GetField(pData, pField, 12, MAXFIELD))
	{
		m_btRMBArrivalStatus = pField[0];
	}
	else
	{
		m_dRMBDestClosingVelocity = 'V';
	}

	m_dwRMBCount++;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessGPRMC(BYTE *pData)
{
	CHAR pBuff[10];
	BYTE pField[MAXFIELD];

	//
	// Time
	//
	if(GetField(pData, pField, 0, MAXFIELD))
	{
		// Hour
		pBuff[0] = pField[0];
		pBuff[1] = pField[1];
		pBuff[2] = '\0';
		m_btRMCHour = atoi(pBuff);

		// minute
		pBuff[0] = pField[2];
		pBuff[1] = pField[3];
		pBuff[2] = '\0';
		m_btRMCMinute = atoi(pBuff);

		// Second
		pBuff[0] = pField[4];
		pBuff[1] = pField[5];
		pBuff[2] = '\0';
		m_btRMCSecond = atoi(pBuff);
	}

	//
	// Data valid
	//
	if(GetField(pData, pField, 1, MAXFIELD))
	{
		m_btRMCDataValid = pField[0];
	}
	else
	{
		m_btRMCDataValid = 'V';
	}

	//
	// latitude
	//
	if(GetField(pData, pField, 2, MAXFIELD))
	{
		m_dRMCLatitude = atof((CHAR *)pField+2) / 60.0;
		pField[2] = '\0';
		m_dRMCLatitude += atof((CHAR *)pField);

	}
	if(GetField(pData, pField, 3, MAXFIELD))
	{
		if(pField[0] == 'S')
		{
			m_dRMCLatitude = -m_dRMCLatitude;
		}
	}

	//
	// Longitude
	//
	if(GetField(pData, pField, 4, MAXFIELD))
	{
		m_dRMCLongitude = atof((CHAR *)pField+3) / 60.0;
		pField[3] = '\0';
		m_dRMCLongitude += atof((CHAR *)pField);
	}
	if(GetField(pData, pField, 5, MAXFIELD))
	{
		if(pField[0] == 'W')
		{
			m_dRMCLongitude = -m_dRMCLongitude;
		}
	}

	//
	// Ground speed
	//
	if(GetField(pData, pField, 6, MAXFIELD))
	{
		m_dRMCGroundSpeed = atof((CHAR *)pField);
	}
	else
	{
		m_dRMCGroundSpeed = 0.0;
	}

	//
	// course over ground, degrees true
	//
	if(GetField(pData, pField, 7, MAXFIELD))
	{
		m_dRMCCourse = atof((CHAR *)pField);
	}
	else
	{
		m_dRMCCourse = 0.0;
	}

	//
	// Date
	//
	if(GetField(pData, pField, 8, MAXFIELD))
	{
		// Day
		pBuff[0] = pField[0];
		pBuff[1] = pField[1];
		pBuff[2] = '\0';
		m_btRMCDay = atoi(pBuff);

		// Month
		pBuff[0] = pField[2];
		pBuff[1] = pField[3];
		pBuff[2] = '\0';
		m_btRMCMonth = atoi(pBuff);

		// Year (Only two digits. I wonder why?)
		pBuff[0] = pField[4];
		pBuff[1] = pField[5];
		pBuff[2] = '\0';
		m_wRMCYear = atoi(pBuff);
		//m_wRMCYear += 2000;				// make 4 digit date -- What assumptions should be made here?
	}

	//
	// course over ground, degrees true
	//
	if(GetField(pData, pField, 9, MAXFIELD))
	{
		m_dRMCMagVar = atof((CHAR *)pField);
	}
	else
	{
		m_dRMCMagVar = 0.0;
	}
	if(GetField(pData, pField, 10, MAXFIELD))
	{
		if(pField[0] == 'W')
		{
			m_dRMCMagVar = -m_dRMCMagVar;
		}
	}

	m_dwRMCCount++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CNMEAParser::ProcessGPZDA(BYTE *pData)
{
	CHAR pBuff[10];
	BYTE pField[MAXFIELD];

	//
	// Time
	//
	if(GetField(pData, pField, 0, MAXFIELD))
	{
		// Hour
		pBuff[0] = pField[0];
		pBuff[1] = pField[1];
		pBuff[2] = '\0';
		m_btZDAHour = atoi(pBuff);

		// minute
		pBuff[0] = pField[2];
		pBuff[1] = pField[3];
		pBuff[2] = '\0';
		m_btZDAMinute = atoi(pBuff);

		// Second
		pBuff[0] = pField[4];
		pBuff[1] = pField[5];
		pBuff[2] = '\0';
		m_btZDASecond = atoi(pBuff);
	}

	//
	// Day
	//
	if(GetField(pData, pField, 1, MAXFIELD))
	{
		m_btZDADay = atoi((CHAR *)pField);
	}
	else
	{
		m_btZDADay = 1;
	}

	//
	// Month
	//
	if(GetField(pData, pField, 2, MAXFIELD))
	{
		m_btZDAMonth = atoi((CHAR *)pField);
	}
	else
	{
		m_btZDAMonth = 1;
	}

	//
	// Year
	//
	if(GetField(pData, pField, 3, MAXFIELD))
	{
		m_wZDAYear = atoi((CHAR *)pField);
	}
	else
	{
		m_wZDAYear = 1;
	}

	//
	// Local zone hour
	//
	if(GetField(pData, pField, 4, MAXFIELD))
	{
		m_btZDALocalZoneHour = atoi((CHAR *)pField);
	}
	else
	{
		m_btZDALocalZoneHour = 0;
	}

	//
	// Local zone hour
	//
	if(GetField(pData, pField, 5, MAXFIELD))
	{
		m_btZDALocalZoneMinute = atoi((CHAR *)pField);
	}
	else
	{
		m_btZDALocalZoneMinute = 0;
	}

	m_dwZDACount++;
}


