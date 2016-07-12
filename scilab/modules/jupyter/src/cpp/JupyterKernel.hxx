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
 ** \file JupyterKernel.hxx
 ** \brief Define class JupyterKernel.
 */

#ifndef JUPYTER_KERNEL_HXX
#define JUPYTER_KERNEL_HXX

#include "JupyterMessage.hxx"
#include "JupyterKernelConnection.hxx"

#include <json/json.h>  // JsonCpp functions
#include <string>
#include <iostream>
#include <fstream>

#include <queue>

#include <thread>
#include <mutex>

#ifndef _WIN32
  #include <unistd.h>
#else
  #include <windows.h>
#endif

/** \brief Define class JupyterKernel.
 **
 ** Contains all the implementation that allows Scilab to be runned.
 ** As a Jupyter protocol (http://jupyter-client.readthedocs.io/en/latest/messaging.html) backend.
 */
class JupyterKernel 
{
public:
  /** \brief Initialize and run Jupyter connection thread.
  ** \param connectionFileName name of the JSON connection configuration file */
  static int Initialize( std::string connectionFileName );
  /** \brief Return pointer to the current command string, received from client. */
  static char* GetInputString();
  /** \brief Send given string to clients.
  ** \param outputString output string to be sent to clients. */
  static void SetOutputString( const char* outputString );
  /** \brief Stop running and end Jupyter connection thread. */
  static void Terminate();
  
private:
  /** \brief Handle shutdown_request message.
  ** \param commandContent JSON object containing message content (shutdown options).
  ** \param replyContent JSON object to be filled with shutdown reply. */
  static void HandleShutdownRequest( Json::Value& commandContent, Json::Value& replyContent );
  /** \brief Handle connection_info_request message.
  ** \param connectionConfig JSON object containing connection configuration.
  ** \param replyContent JSON object to be filled with relevant connection information. */
  static void FillConnectionInfo( Json::Value& connectionConfig, Json::Value& replyContent );
  // Message handler function declarations
  /** \brief Handle kernel_info_request message.
  ** \param replyContent JSON object to be filled with kernel information. */
  static void FillKernelInfo( Json::Value& replyContent );
  /** \brief Handle history_request message.
  ** \param commandContent JSON object containing message content (history options).
  ** \param replyContent JSON object to be filled with requested history information. */
  static void HandleHistoryRequest( Json::Value& commandContent, Json::Value& replyContent );
  /** \brief Handle inspect_request message.
  ** \param commandContent JSON object containing message content (code/object name).
  ** \param replyContent JSON object to be filled with requested object information. */
  static void HandleInspectionRequest( Json::Value& commandContent, Json::Value& replyContent );
  /** \brief Handle complete_request message.
  ** \param commandContent JSON object containing message content (partial code).
  ** \param replyContent JSON object to be filled with possible completions dictionary. */
  static void HandleCompletionRequest( Json::Value& commandContent, Json::Value& replyContent );
  /** \brief Handle is_complete_request message.
  ** \param commandMessage Jupyter message object containing command to be parsed.
  ** \param replyContent JSON object to be filled with completeness status. */
  static void HandleCompletenessRequest( JupyterMessage& commandMessage, Json::Value& replyContent );
  
  /** \brief Handle execution_request message.
  ** \param publisher Jupyter connection reference (for publishing execution updates).
  ** \param commandMessage Jupyter message object containing command to be replied. */
  static void HandleExecutionRequest( JupyterKernelConnection& publisher, JupyterMessage& commandMessage );
  /** \brief Capture execution output and send execution_reply message.
  ** \param publisher Jupyter connection reference (for publishing execution updates).
  ** \param commandMessage Jupyter message object containing command to be replied (from previous request). */
  static void HandleExecutionReply( JupyterKernelConnection& publisher, JupyterMessage commandMessage );
  
  /** \brief Publish all non execution results to stream (stdout) Jupyter topic.
  ** \param publisher Jupyter connection reference (for publishing update). */
  static void FlushOutputStream( JupyterKernelConnection& publisher );
  
  /** \brief Send input_request message to the identified shell client for getting user input.
  ** \param requester Jupyter connection used for sending the request the message.
  ** \param shellIdentity Identifier of the client shell from which input is being requested. */
  static void HandleInputRequest( JupyterKernelConnection& requester, std::string shellIdentity );
  /** \brief Handle input_reply message.
  ** \param replyContent JSON object to be filled with user input content. */
  static void HandleInputReply( Json::Value& replyContent );
  
  /** Jupyter connection update (send/receive) thread handle. */
  static std::thread updateThread;
  /** \brief Jupyter connection update (send/receive) thread function.
  ** \param connectionConfig JSON object containing connection configuration. */
  static void RunUpdateThread( Json::Value connectionConfig );
  /** Defines if command processing is being performed. */
  static volatile bool isProcessing, isWaitingInput, isInputAllowed;
  
  /** Thread lock for waiting previous command to be processed before ovewriting. */
  static std::mutex inputLock;
  /** Current command being processed. */
  static std::string inputCurrentString;
  
  /** Thread lock for synchronizing output queue access. */
  static std::mutex outputLock;
  /** Output strings (sent to clients) queue. */
  static std::queue<std::string> outputQueue;
  /** Helper function. Flushes and concatenates the output queue into a single string. */
  static std::string GetCurrentOutput();
  
  /** User name to identify kernel messages. */
  static const std::string USER_NAME;
};

#endif // JUPYTER_KERNEL_HXX
