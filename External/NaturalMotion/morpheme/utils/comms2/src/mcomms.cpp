// Copyright (c) 2010 NaturalMotion.  All Rights Reserved.
// Not to be copied, adapted, modified, used, distributed, sold,
// licensed or commercially exploited in any manner without the
// written consent of NaturalMotion.
//
// All non public elements of this software are the confidential
// information of NaturalMotion and may not be disclosed to any
// person nor used for any purpose not expressly approved by
// NaturalMotion in writing.

#include "comms/mcomms.h"
#include "comms/packet.h"
#include "comms/corePackets.h"
#include "comms/commsServer.h"
#include "NMPlatform/NMPlatform.h"
#include "NMPlatform/NMMemory.h"
#include "NMPlatform/NMSocketWrapper.h"

#if !defined(NM_HOST_CELL_SPU) && !defined(NM_HOST_CELL_PPU)
  #include <stdio.h>
#endif //#if !defined(NM_HOST_CELL_SPU) && !defined(NM_HOST_CELL_PPU)

// Disable warnings constant conditionals
// Generated by calling FD_SET() in winsock.h
#ifdef NM_COMPILER_MSVC
  #pragma warning (push)
  #pragma warning(disable : 4127)
#endif

#ifdef NMVERBOSE
  #include <stdio.h>
#endif

namespace MCOMMS
{

//----------------------------------------------------------------------------------------------------------------------
bool GUID::operator==(const GUID& other) const
{
  for (uint32_t i = 0; i < 16; ++i)
  {
    if (value[i] != other.value[i])
    {
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool GUID::operator<(const GUID& other) const
{
  for (uint32_t i = 0; i < 16; ++i)
  {
    if (value[i] < other.value[i])
    {
      return true;
    }
    else if (value[i] > other.value[i])
    {
      return false;
    }
  }
  return false;
}

//----------------------------------------------------------------------------------------------------------------------
void GUID::toStringGUID(char* stringGUID) const
{
  char hex2Int[16];
  hex2Int[0] = '0';  hex2Int[1] = '1';  hex2Int[2] = '2';  hex2Int[3] = '3';
  hex2Int[4] = '4';  hex2Int[5] = '5';  hex2Int[6] = '6';  hex2Int[7] = '7';
  hex2Int[8] = '8';  hex2Int[9] = '9';  hex2Int[10] = 'a'; hex2Int[11] = 'b';
  hex2Int[12] = 'c'; hex2Int[13] = 'd'; hex2Int[14] = 'e'; hex2Int[15] = 'f';

  char* result;
  result = stringGUID;
#if defined(NM_COMPILER_MSVC)
  strcpy_s(result, 37, "00000000-0000-0000-0000-000000000000");
#else
  strcpy(result, "00000000-0000-0000-0000-000000000000");
#endif
  uint32_t strLoc = 0;
  for (uint32_t comVLoc = 0; comVLoc < 16; comVLoc++)
  {
    if (strLoc == 8 || strLoc == 13 || strLoc == 18 || strLoc == 23)
    {
      result[strLoc] = L'-';
      strLoc++;
    }
    unsigned char val1 = value[comVLoc] & 0x0F;
    unsigned char val0 = value[comVLoc] >> 4;
    result[strLoc] = hex2Int[val0];
    strLoc++;
    result[strLoc] = hex2Int[val1];
    strLoc++;
  }
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t GUID::hashStringGuid(const char* strGuid) const
{
  #define MOD_ADLER 65521
  uint8_t* data = (uint8_t*)strGuid;
  size_t len = strlen(strGuid);
  uint32_t a = 1, b = 0;

  while (len)
  {
    size_t tlen = len > 5550 ? 5550 : len;
    len -= tlen;

    do
    {
      a += *data++;
      b += a;
    } while (--tlen);

    a = (a & 0xffff) + (a >> 16) * (65536 - MOD_ADLER);
    b = (b & 0xffff) + (b >> 16) * (65536 - MOD_ADLER);
  }
  // It can be shown that a <= 0x1013a here, so a single subtract will do.
  if (a >= MOD_ADLER)
    a -= MOD_ADLER;
  // It can be shown that b can reach 0xffef1 here.
  b = (b & 0xffff) + (b >> 16) * (65536 - MOD_ADLER);
  if (b >= MOD_ADLER)
    b -= MOD_ADLER;
  return (b << 16) | a;
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t sendBuffer(NMP::SocketWrapper* socket, void* buffer, uint32_t bufferSize)
{
  int bytes;
  int size = bufferSize;
  const char* buf = (const char*)buffer;

  while (size > 0)
  {
    bytes = NMP::send(socket->getSocket(), buf, size, 0);
    if (bytes <= 0)
      break;

    size -= bytes;
    buf += bytes;
  }

  return bufferSize - size;
}

//----------------------------------------------------------------------------------------------------------------------
// The following function takes a socket object and is _only_ used by the socket-taking function.
// It's used only when activity presence has been already checked, so if this returns 0-read bytes
// we can assume a network error.
uint32_t recvBuffer(NMP::SocketWrapper* socket, void* buffer, uint32_t bufferSize)
{
  int bytes;
  int size = bufferSize;
  char* buf = (char*)buffer;

  while (size > 0)
  {
    bytes = NMP::recv(socket->getSocket(), buf, size, 0);
    if (bytes <= 0)
    {
      break;
    }

    size -= bytes;
    buf += bytes;
  }

  return bufferSize - size;
}

//----------------------------------------------------------------------------------------------------------------------
/// Log some debug information about the packet.
void logPacket(PacketBase* pkt)
{
#ifdef MCOMMS_VERBOSE
  switch (pkt->hdr.m_id)
  {
  case pk_Ping:
    NMP_MSG("MorphemeComms: Received ping %d", ((PingPacket*)pkt)->m_pingId);
    break;

  default:
    {
      NMP_MSG("MorphemeComms: Received packet - Id: %d - Len: %d - Category: %d",
              pkt->hdr.m_id, pkt->hdr.m_length, pkt->hdr.m_magicB);
    }
    break;
  }
#else
  (void)pkt;
#endif
}

//----------------------------------------------------------------------------------------------------------------------
const char* guidToString(const GUID* guid)
{
#if !defined(NM_HOST_CELL_SPU) && !defined(NM_HOST_CELL_PPU)
  static char guidBuffer[70];
  char* cur = guidBuffer;
  for (uint32_t i = 0 ; i < sizeof(guid->value) ; ++i)
  {
    // NB That the following is printing 3 characters and a null, so will go over the end
    // if the above buffer is 64 bytes.
    int printed = NMP_SPRINTF(cur, 4, "%02x ", guid->value[i]);
    cur += printed;
  }
  return guidBuffer;
#else
  (void)guid;
  return "";
#endif
}

//----------------------------------------------------------------------------------------------------------------------
RuntimeTargetInterface* getRuntimeTarget()
{
  CommsServer* commsServer = CommsServer::getInstance();
  if (commsServer)
  {
    return commsServer->getRuntimeTargetInterface();
  }
  return 0;
}

//----------------------------------------------------------------------------------------------------------------------
Vec3 vec3fromVector3(const NMP::Vector3& vec)
{
  Vec3 out;
  out.v[0] = vec.x;
  out.v[1] = vec.y;
  out.v[2] = vec.z;

  return out;
}

//----------------------------------------------------------------------------------------------------------------------
Vec4 vec4fromQuat(const NMP::Quat& quat)
{
  Vec4 out;
  out.v[0] = quat.x;
  out.v[1] = quat.y;
  out.v[2] = quat.z;
  out.v[3] = quat.w;
  return out;
}

//----------------------------------------------------------------------------------------------------------------------
NMP::Vector3 vector3fromVec3(const Vec3& vec)
{
  NMP::Vector3 out;
  out.x = vec.v[0];
  out.y = vec.v[1];
  out.z = vec.v[2];

  return out;
}

//----------------------------------------------------------------------------------------------------------------------
NMP::Quat quatfromVec4(const Vec4& vec)
{
  NMP::Quat out;
  out.x = vec.v[0];
  out.y = vec.v[1];
  out.z = vec.v[2];
  out.w = vec.v[3];
  return out;
}

//----------------------------------------------------------------------------------------------------------------------
NMP::Matrix34 matrix34fromMat34(const Mat34& tm)
{
  NMP::Matrix34 out;
  memset(&out, 0, sizeof(NMP::Matrix34));

  for (int i = 0; i < 3; ++i)
    out.r[0][i] = tm.m_row[0].v[i];
  for (int i = 0; i < 3; ++i)
    out.r[1][i] = tm.m_row[1].v[i];
  for (int i = 0; i < 3; ++i)
    out.r[2][i] = tm.m_row[2].v[i];
  for (int i = 0; i < 3; ++i)
    out.r[3][i] = tm.m_row[3].v[i];
  return out;
}

//----------------------------------------------------------------------------------------------------------------------
Mat34 mat34fromMatrix34(const NMP::Matrix34& tm)
{
  Mat34 out;
  memset(&out, 0, sizeof(Mat34));

  for (int i = 0; i < 3; ++i)
    out.m_row[0].v[i] = tm.r[0][i];
  for (int i = 0; i < 3; ++i)
    out.m_row[1].v[i] = tm.r[1][i];
  for (int i = 0; i < 3; ++i)
    out.m_row[2].v[i] = tm.r[2][i];
  for (int i = 0; i < 3; ++i)
    out.m_row[3].v[i] = tm.r[3][i];
  return out;
}

//----------------------------------------------------------------------------------------------------------------------
SocketStatus checkSocketActivity(NMP::SocketWrapper* socketWrapper)
{
  NMP::fd_set readSockets;
  FD_ZERO(&readSockets);

  if (!socketWrapper->isValid())
  {
    return SOCKET_NOT_CONNECTED;
  }

  FD_SET(socketWrapper->getSocket(), &readSockets);
  int socketsWithMessages = NMP::getSocketsWithMessages(1, &readSockets, 0, 0, 100);

  // Check for errors. This is not a connection specific error.
  if (socketsWithMessages == -1)
  {
    return SOCKET_ERROR_OCCURRED;
  }

  if (FD_ISSET(socketWrapper->getSocket(), &readSockets))
  {
    return SOCKET_HAS_MESSAGES;
  }

  return SOCKET_NO_MESSAGES;
}

} // namespace MCOMMS

//----------------------------------------------------------------------------------------------------------------------
#ifdef NM_COMPILER_MSVC
  #pragma warning (pop)
#endif
//----------------------------------------------------------------------------------------------------------------------