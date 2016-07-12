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

/**
 ** \file JupyterMessage.hxx
 ** \brief Define class JupyterMessage.
 */

#ifndef JUPYTER_MESSAGE_HXX
#define JUPYTER_MESSAGE_HXX

// JsonCpp functions
#include <json/json.h>  
#include <json/reader.h>
#include <json/writer.h>

#include <string>


/**
 ** \brief Define class JupyterMessage.
 **
 ** Encapsulates Jupyter protocol multipart JSON messages, providing
 ** methods for data serialization/deserialization, update and other
 ** helper functions.
 */
class JupyterMessage
{
public:
  /** \brief Default (request) message constructor.
  ** \param userName message sender identifier
  ** \param messageType message type identifier (topic for published ones) */
  JupyterMessage( std::string userName = "", std::string messageType = "" );
  /** \brief Fill serialize data strings from internal JSON objects data.
  ** \param serialHeader serialized header data storage.
  ** \param serialParent serialized parent data storage.
  ** \param serialMetadata serialized metadata data storage.
  ** \param serialContent serialized content data storage. */
  void SerializeData( std::string& serialHeader, std::string& serialParent, std::string& serialMetadata, std::string& serialContent );
  /** \brief Fill internal JSON objects data from serialized data strings.
  ** \param serialHeader serialized header data input.
  ** \param serialParent serialized parent data input.
  ** \param serialMetadata serialized metadata data input.
  ** \param serialContent serialized content data input. */
  void DeserializeData( std::string& serialHeader, std::string& serialParent, std::string& serialMetadata, std::string& serialContent );
  /** \brief Get message type identifier string.
  ** \return message type string. */
  std::string GetTypeString();
  /** \brief Generate properly filled reply for original message.
  ** \param userName message sender identifier
  ** \param messageType message type identifier (if omitted, original "_request" suffix is converted to "_reply") 
  ** \return reply message object */
  JupyterMessage GenerateReply( std::string userName, std::string messageType = "" );
  /** \brief Define unique identifier (UUID) for message session.
  **  \param sessionUUID unique identifier string. */
  void SetSessionUUID( std::string sessionUUID );
  /** \brief Set message time stamp string to ISO format current time. */
  void UpdateTimeStamp();
  
  /** Message type identifier string. */
  std::string identifier;
  /** Public JSON object message parts. */
  Json::Value metadata, content;
  
  /** Defaul string delimiter between messages. */
  static const char* DELIMITER;
  
private: 
  /** Private JSON object message parts. */
  Json::Value header, parent;
  /** JSON object serializer (formats object data to string). */
  Json::FastWriter serializer;
  /** JSON object deserializer (parses string data to structured object). */
  Json::Reader deserializer;
  
  /** Protocol version string identifier */
  static const std::string VERSION_NUMBER;
};

#endif // JUPYTER_MESSAGE_HXX
