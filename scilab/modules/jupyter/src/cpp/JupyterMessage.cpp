/*
 *  Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 *  Copyright (C) 2016 - Leonardo José Consoni
 *
 * Copyright (C) 2012 - 2016 - Scilab Enterprises
 *
 * This file is hereby licensed under the terms of the GNU GPL v2.0,
 * pursuant to article 5.3.4 of the CeCILL v.2.1.
 * This file was originally licensed under the terms of the CeCILL v2.1,
 * and continues to be available under such terms.
 * For more information, see the COPYING file which you should have received
 * along with this program.
 *
 */

#include "JupyterMessage.hxx"

#include "uuid.hxx"

#include <ctime>
#include <cstring>
#include <sys/time.h>

#include <iostream>

const char* JupyterMessage::DELIMITER = "<IDS|MSG>";

const std::string JupyterMessage::VERSION_NUMBER = "5.0";

JupyterMessage::JupyterMessage( std::string userName, std::string messageType )
{
  identifier = messageType;
  
  header[ "msg_type" ] = messageType;
  header[ "username" ] = userName;
  header[ "version" ] = VERSION_NUMBER;
  header[ "msg_id" ] = GenerateNewUUID();
  
  parent = Json::Value( Json::objectValue );
  metadata = Json::Value( Json::objectValue );
  content = Json::Value( Json::objectValue );
}

void JupyterMessage::SerializeData( std::string& serialHeader, std::string& serialParent, std::string& serialMetadata, std::string& serialContent )
{
  serialHeader = serializer.write( header );   
  serialParent = serializer.write( parent );   
  serialMetadata = serializer.write( metadata );   
  serialContent = serializer.write( content );
}

void JupyterMessage::DeserializeData( std::string& serialHeader, std::string& serialParent, std::string& serialMetadata, std::string& serialContent )
{
  deserializer.parse( serialHeader, header, false );        // Header  
  deserializer.parse( serialParent, parent, false );        // Parent Header
  deserializer.parse( serialMetadata, metadata, false );    // Metadata
  deserializer.parse( serialContent, content, false );      // Content
}

std::string JupyterMessage::GetTypeString() { return header.get( "msg_type", "" ).asString(); }

JupyterMessage JupyterMessage::GenerateReply( std::string userName, std::string messageType )
{
  JupyterMessage replyMessage( userName, messageType );
  
  if( messageType.empty() )
  {
    messageType = GetTypeString();
    size_t requestSuffix = messageType.find( "_request" );
    
    if( requestSuffix != std::string::npos )
      messageType.replace( requestSuffix, std::string::npos, "_reply" );
    
    replyMessage.header[ "msg_type" ] = messageType;
    replyMessage.identifier = identifier;
  }
  
  replyMessage.parent = header;
  
  return replyMessage;
}

void JupyterMessage::SetSessionUUID( std::string sessionUUID )
{
  header[ "session" ] = sessionUUID;
}

void JupyterMessage::UpdateTimeStamp()
{
  struct timeval timeStamp;
  gettimeofday( &timeStamp, NULL );
  char timeStampString[ 100 ];
  strftime( (char*) timeStampString, 100, "%FT%T", std::localtime( &timeStamp.tv_sec ) );
  sprintf( timeStampString + strlen( timeStampString ), ".%3lu", timeStamp.tv_usec );
  header[ "date" ] = timeStampString;
}
