/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <semaphore.h>

#include "com_android_nfc.h"

static phNfc_sData_t *com_android_nfc_jni_transceive_buffer;
static phNfc_sData_t *com_android_nfc_jni_ioctl_buffer;
static phNfc_sRemoteDevInformation_t* SecureElementInfo;
static int secureElementHandle;
extern void                 *gHWRef;
static int SecureElementTech;

namespace android {

static void com_android_nfc_jni_ioctl_callback ( void*            pContext,
                                            phNfc_sData_t*   Outparam_Cb,
                                            NFCSTATUS        status)
{
    struct nfc_jni_callback_data * pContextData =  (struct nfc_jni_callback_data*)pContext;

	if (status == NFCSTATUS_SUCCESS )
	{
		LOGD("> IOCTL successful");
	}
	else
	{
		LOGD("> IOCTL error");
	}

   com_android_nfc_jni_ioctl_buffer = Outparam_Cb;
   pContextData->status = status;
   sem_post(&pContextData->sem);
}

static void com_android_nfc_jni_transceive_callback(void *pContext,
   phLibNfc_Handle handle, phNfc_sData_t *pResBuffer, NFCSTATUS status)
{
   struct nfc_jni_callback_data * pContextData =  (struct nfc_jni_callback_data*)pContext;

   LOG_CALLBACK("com_android_nfc_jni_transceive_callback", status);
  
   com_android_nfc_jni_transceive_buffer = pResBuffer;
   pContextData->status = status;
   sem_post(&pContextData->sem);
}


static void com_android_nfc_jni_connect_callback(void *pContext,
                                            phLibNfc_Handle hRemoteDev,
                                            phLibNfc_sRemoteDevInformation_t *psRemoteDevInfo, NFCSTATUS status)
{
   struct nfc_jni_callback_data * pContextData =  (struct nfc_jni_callback_data*)pContext;

   LOG_CALLBACK("com_android_nfc_jni_connect_callback", status);

   pContextData->status = status;
   sem_post(&pContextData->sem);
}

static void com_android_nfc_jni_disconnect_callback(void *pContext,
                                               phLibNfc_Handle hRemoteDev, 
                                               NFCSTATUS status)
{
   struct nfc_jni_callback_data * pContextData =  (struct nfc_jni_callback_data*)pContext;

   LOG_CALLBACK("com_android_nfc_jni_disconnect_callback", status);
   
   pContextData->status = status;
   sem_post(&pContextData->sem);
}

/* Set Secure Element mode callback*/
static void com_android_nfc_jni_smartMX_setModeCb (void*            pContext,
							                                phLibNfc_Handle  hSecureElement,
                                              NFCSTATUS        status)
{      
    struct nfc_jni_callback_data * pContextData =  (struct nfc_jni_callback_data*)pContext;

	if(status==NFCSTATUS_SUCCESS)
	{
		LOGD("SE Set Mode is Successful");
		LOGD("SE Handle: %lu", hSecureElement);		
	}
	else
	{
    LOGD("SE Set Mode is failed\n ");
  }
	
   pContextData->status = status;
   sem_post(&pContextData->sem);
}

static void com_android_nfc_jni_open_secure_element_notification_callback(void *pContext,
                                                                     phLibNfc_RemoteDevList_t *psRemoteDevList,
                                                                     uint8_t uNofRemoteDev, 
                                                                     NFCSTATUS status)
{
   struct nfc_jni_callback_data * pContextData =  (struct nfc_jni_callback_data*)pContext;
   NFCSTATUS ret;
   int i;
   
   if(status == NFCSTATUS_DESELECTED)
   {
      LOG_CALLBACK("com_android_nfc_jni_open_secure_element_notification_callback: Target deselected", status);
   }
   else
   {   
      LOG_CALLBACK("com_android_nfc_jni_open_secure_element_notification_callback", status);
      LOGI("Discovered %d tags", uNofRemoteDev);
      
      if(status == NFCSTATUS_MULTIPLE_PROTOCOLS)
      {	
         LOGD("Multiple Protocol supported\n");
                                 
         LOGD("Secure Element Handle: 0x%08x",psRemoteDevList[1].hTargetDev);
         secureElementHandle = psRemoteDevList[1].hTargetDev;
         
         /* Set type name */
         jintArray techList;
         jintArray handleList;
         jintArray typeList;
         nfc_jni_get_technology_tree(pContextData->e, psRemoteDevList,uNofRemoteDev, &techList,
                 &handleList, &typeList);
         // TODO: Should use the "connected" technology, for now use the first
         if (pContextData->e->GetArrayLength(techList) > 0) {
             jint* technologies = pContextData->e->GetIntArrayElements(techList, 0);
             SecureElementTech = technologies[0];
             LOGD("Store Secure Element Info\n");
             SecureElementInfo = psRemoteDevList->psRemoteDevInfo;

             LOGD("Discovered secure element: tech=%d", SecureElementTech);
         }
         else {
             LOGE("Discovered secure element, but could not resolve tech");
             status = NFCSTATUS_FAILED;
         }
    
      }
      else
      {
         LOGD("Secure Element Handle: 0x%08x",psRemoteDevList->hTargetDev);
         secureElementHandle = psRemoteDevList->hTargetDev;
         
         /* Set type name */      
         jintArray techList;
         jintArray handleList;
         jintArray typeList;
         nfc_jni_get_technology_tree(pContextData->e, psRemoteDevList,uNofRemoteDev, &techList,
                 &handleList, &typeList);

         // TODO: Should use the "connected" technology, for now use the first
         if ((techList != NULL) && pContextData->e->GetArrayLength(techList) > 0) {
             jint* technologies = pContextData->e->GetIntArrayElements(techList, 0);
             SecureElementTech = technologies[0];
             LOGD("Store Secure Element Info\n");
             SecureElementInfo = psRemoteDevList->psRemoteDevInfo;

             LOGD("Discovered secure element: tech=%d", SecureElementTech);
         }
         else {
             LOGE("Discovered secure element, but could not resolve tech");
             status = NFCSTATUS_FAILED;
         }
      }     
   }
         
   pContextData->status = status;
   sem_post(&pContextData->sem);
}


static jint com_android_nfc_NativeNfcSecureElement_doOpenSecureElementConnection(JNIEnv *e, jobject o)
{
   NFCSTATUS ret;
   int semResult;
   
   phLibNfc_SE_List_t SE_List[PHLIBNFC_MAXNO_OF_SE];
   uint8_t i, No_SE = PHLIBNFC_MAXNO_OF_SE, SmartMX_index=0, SmartMX_detected = 0;
   phLibNfc_sADD_Cfg_t discovery_cfg;
   phLibNfc_Registry_Info_t registry_info;
   phNfc_sData_t			InParam;
   phNfc_sData_t			OutParam;
   uint8_t              ExternalRFDetected[3] = {0x00, 0xFC, 0x01};
   uint8_t					Output_Buff[50];
   uint8_t              reg_value;
   uint8_t              mask_value;
   struct nfc_jni_callback_data cb_data;
   
   /* Create the local semaphore */
   if (!nfc_cb_data_init(&cb_data, NULL))
   {
      goto clean_and_return;
   }

   cb_data.e = e;

   /* Registery */   
   registry_info.MifareUL = TRUE;
   registry_info.MifareStd = TRUE;
   registry_info.ISO14443_4A = TRUE;
   registry_info.ISO14443_4B = TRUE;
   registry_info.Jewel = TRUE;
   registry_info.Felica = TRUE;  
   registry_info.NFC = FALSE;  
     
   CONCURRENCY_LOCK();
   
   LOGD("Open Secure Element");
   
   /* Test if External RF field is detected */
	InParam.buffer = ExternalRFDetected;
	InParam.length = 3;
	OutParam.buffer = Output_Buff;
	LOGD("phLibNfc_Mgt_IoCtl()");
   ret = phLibNfc_Mgt_IoCtl(gHWRef,NFC_MEM_READ,&InParam, &OutParam,com_android_nfc_jni_ioctl_callback, (void *)&cb_data);
   if(ret!=NFCSTATUS_PENDING)
	{
      LOGE("IOCTL status error");
	}

   /* Wait for callback response */
   if(sem_wait(&cb_data.sem))
   {
      LOGE("IOCTL semaphore error");
      goto clean_and_return;
   }
      
   if(cb_data.status != NFCSTATUS_SUCCESS)
   {
      LOGE("READ MEM ERROR");
      goto clean_and_return;
   }
   
   /* Check the value */   
   reg_value = com_android_nfc_jni_ioctl_buffer->buffer[0];
   mask_value = reg_value & 0x40;

   if(mask_value == 0x40)
   {
      LOGD("External RF Field detected");
      goto clean_and_return;   
   }   
   
   /* Get Secure Element List */
   LOGD("phLibNfc_SE_GetSecureElementList()");
   ret = phLibNfc_SE_GetSecureElementList( SE_List, &No_SE);
   if (ret == NFCSTATUS_SUCCESS)
   {
      LOGD("\n> Number of Secure Element(s) : %d\n", No_SE);
      /* Display Secure Element information */
      for (i = 0; i<No_SE; i++)
      {
         if (SE_List[i].eSE_Type == phLibNfc_SE_Type_SmartMX)
         {
           LOGD("> SMX detected");
           LOGD("> Secure Element Handle : %d\n", SE_List[i].hSecureElement);
           /* save SMARTMX index */
           SmartMX_detected = 1;
           SmartMX_index = i;
         }
      }
       
      if(SmartMX_detected)
      {
         REENTRANCE_LOCK();
         LOGD("phLibNfc_RemoteDev_NtfRegister()");
         ret = phLibNfc_RemoteDev_NtfRegister(&registry_info, com_android_nfc_jni_open_secure_element_notification_callback, (void *)&cb_data);
         REENTRANCE_UNLOCK();
         if(ret != NFCSTATUS_SUCCESS)
         {
            LOGW("Register Notification error");
            goto clean_and_return;
         }
      
         /* Set wired mode */
         REENTRANCE_LOCK();
         LOGD("phLibNfc_SE_SetMode: Wired mode");
         ret = phLibNfc_SE_SetMode( SE_List[SmartMX_index].hSecureElement, 
									         phLibNfc_SE_ActModeWired, 
									         com_android_nfc_jni_smartMX_setModeCb,
									         (void *)&cb_data);
         REENTRANCE_UNLOCK();
         if (ret != NFCSTATUS_PENDING )
         {
            LOGD("\n> SE Set SmartMX mode ERROR \n" );
            goto clean_and_return;
         }
			
         /* Wait for callback response */
         if(sem_wait(&cb_data.sem))
         {
            LOGW("Secure Element opening error");
            goto clean_and_return;
         }
      
         if(cb_data.status != NFCSTATUS_SUCCESS)
         {
            LOGE("SE set mode failed");
            goto clean_and_return;
         }
      
         LOGD("Waiting for notification");
         /* Wait for callback response */
         if(sem_wait(&cb_data.sem))
         {
            LOGW("Secure Element opening error");
            goto clean_and_return;
         }
      
         if(cb_data.status != NFCSTATUS_SUCCESS && cb_data.status != NFCSTATUS_MULTIPLE_PROTOCOLS)
         {
            LOGE("SE detection failed");
            goto clean_and_return;
         }
         CONCURRENCY_UNLOCK();
         
         /* Connect Tag */
         CONCURRENCY_LOCK();
         LOGD("phLibNfc_RemoteDev_Connect(SMX)");
         REENTRANCE_LOCK();
         ret = phLibNfc_RemoteDev_Connect(secureElementHandle, com_android_nfc_jni_connect_callback,(void *)&cb_data);
         REENTRANCE_UNLOCK();
         if(ret != NFCSTATUS_PENDING)
         {
            LOGE("phLibNfc_RemoteDev_Connect(SMX) returned 0x%04x[%s]", ret, nfc_jni_get_status_name(ret));
            goto clean_and_return;
         }
         LOGD("phLibNfc_RemoteDev_Connect(SMX) returned 0x%04x[%s]", ret, nfc_jni_get_status_name(ret));
      
         /* Wait for callback response */
         if(sem_wait(&cb_data.sem))
         {
             goto clean_and_return;
         }
         
         /* Connect Status */
         if(cb_data.status != NFCSTATUS_SUCCESS)
         {
            goto clean_and_return;
         }
         
         CONCURRENCY_UNLOCK();
         /* Return the Handle of the SecureElement */         
         return secureElementHandle;
      }
      else
      {
         LOGD("phLibNfc_SE_GetSecureElementList(): No SMX detected");   
         goto clean_and_return; 
      } 
  }
  else
  {
      LOGD("phLibNfc_SE_GetSecureElementList(): Error");
      goto clean_and_return;
  }
  
clean_and_return:
   CONCURRENCY_UNLOCK();
   return 0;
}


static jboolean com_android_nfc_NativeNfcSecureElement_doDisconnect(JNIEnv *e, jobject o, jint handle)
{
   jclass cls;
   jfieldID f;
   NFCSTATUS status;
   jboolean result = JNI_FALSE;
   phLibNfc_SE_List_t SE_List[PHLIBNFC_MAXNO_OF_SE];
   uint8_t i, No_SE = PHLIBNFC_MAXNO_OF_SE, SmartMX_index=0, SmartMX_detected = 0;
   uint32_t SmartMX_Handle;
   struct nfc_jni_callback_data cb_data;

   /* Create the local semaphore */
   if (!nfc_cb_data_init(&cb_data, NULL))
   {
      goto clean_and_return;
   }

   LOGD("Close Secure element function ");
   
   CONCURRENCY_LOCK();
   /* Disconnect */
   LOGI("Disconnecting from SMX (handle = 0x%x)", handle);
   REENTRANCE_LOCK();
   status = phLibNfc_RemoteDev_Disconnect(handle, 
                                          NFC_SMARTMX_RELEASE,
                                          com_android_nfc_jni_disconnect_callback,
                                          (void *)&cb_data);
   REENTRANCE_UNLOCK();
   if(status != NFCSTATUS_PENDING)
   {
      LOGE("phLibNfc_RemoteDev_Disconnect(SMX) returned 0x%04x[%s]", status, nfc_jni_get_status_name(status));
      goto clean_and_return;
   }
   LOGD("phLibNfc_RemoteDev_Disconnect(SMX) returned 0x%04x[%s]", status, nfc_jni_get_status_name(status));

   /* Wait for callback response */
   if(sem_wait(&cb_data.sem))
   {
       goto clean_and_return;
   }
   
   /* Disconnect Status */
   if(cb_data.status != NFCSTATUS_SUCCESS)
   {
     LOGE("\n> Disconnect SE ERROR \n" );
      goto clean_and_return;
   }
   
   result = JNI_TRUE;

clean_and_return:
   CONCURRENCY_UNLOCK();
   return result;
}

static jbyteArray com_android_nfc_NativeNfcSecureElement_doTransceive(JNIEnv *e,
   jobject o,jint handle, jbyteArray data)
{
   uint8_t offset = 0;
   uint8_t *buf;
   uint32_t buflen;
   phLibNfc_sTransceiveInfo_t transceive_info;
   jbyteArray result = NULL;
   int res; 
   
   int tech = SecureElementTech;
   NFCSTATUS status;
   struct nfc_jni_callback_data cb_data;

   /* Create the local semaphore */
   if (!nfc_cb_data_init(&cb_data, NULL))
   {
      goto clean_and_return;
   }

   LOGD("Exchange APDU function ");
   
   CONCURRENCY_LOCK();
   
   LOGD("Secure Element tech: %d\n", tech);

   buf = (uint8_t *)e->GetByteArrayElements(data, NULL);
   buflen = (uint32_t)e->GetArrayLength(data);
 
   /* Prepare transceive info structure */
   if(tech == TARGET_TYPE_MIFARE_CLASSIC || tech == TARGET_TYPE_MIFARE_UL)
   {
      offset = 2;
      transceive_info.cmd.MfCmd = (phNfc_eMifareCmdList_t)buf[0];
      transceive_info.addr = (uint8_t)buf[1];
   }
   else if(tech == TARGET_TYPE_ISO14443_4)
   {
      transceive_info.cmd.Iso144434Cmd = phNfc_eIso14443_4_Raw;
      transceive_info.addr = 0;
   }
      
   transceive_info.sSendData.buffer = buf + offset;
   transceive_info.sSendData.length = buflen - offset;
   transceive_info.sRecvData.buffer = (uint8_t*)malloc(1024);
   transceive_info.sRecvData.length = 1024;

   if(transceive_info.sRecvData.buffer == NULL)
   {
      goto clean_and_return;
   }

   LOGD("phLibNfc_RemoteDev_Transceive(SMX)");
   REENTRANCE_LOCK();
   status = phLibNfc_RemoteDev_Transceive(handle, &transceive_info,
		   com_android_nfc_jni_transceive_callback, (void *)&cb_data);
   REENTRANCE_UNLOCK();
   if(status != NFCSTATUS_PENDING)
   {
      LOGE("phLibNfc_RemoteDev_Transceive(SMX) returned 0x%04x[%s]", status, nfc_jni_get_status_name(status));
      goto clean_and_return;
   }
   LOGD("phLibNfc_RemoteDev_Transceive(SMX) returned 0x%04x[%s]", status, nfc_jni_get_status_name(status));

   /* Wait for callback response */
   if(sem_wait(&cb_data.sem))
   {
       goto clean_and_return;
   }

   if(cb_data.status != NFCSTATUS_SUCCESS)
   {
      goto clean_and_return;
   }

   /* Copy results back to Java */
   result = e->NewByteArray(com_android_nfc_jni_transceive_buffer->length);
   if(result != NULL)
   {
      e->SetByteArrayRegion(result, 0,
    		  com_android_nfc_jni_transceive_buffer->length,
         (jbyte *)com_android_nfc_jni_transceive_buffer->buffer);
   }

clean_and_return:
   if(transceive_info.sRecvData.buffer != NULL)
   {
      free(transceive_info.sRecvData.buffer);
   }

   e->ReleaseByteArrayElements(data,
      (jbyte *)transceive_info.sSendData.buffer, JNI_ABORT);

   CONCURRENCY_UNLOCK();

   return result;
}

static jbyteArray com_android_nfc_NativeNfcSecureElement_doGetUid(JNIEnv *e, jobject o, jint handle)
{
   LOGD("Get Secure element UID function ");
   jbyteArray SecureElementUid;
      
   if(handle == secureElementHandle)
   {
      SecureElementUid = e->NewByteArray(SecureElementInfo->RemoteDevInfo.Iso14443A_Info.UidLength);
      e->SetByteArrayRegion(SecureElementUid, 0, SecureElementInfo->RemoteDevInfo.Iso14443A_Info.UidLength,(jbyte *)SecureElementInfo->RemoteDevInfo.Iso14443A_Info.Uid);
      return SecureElementUid;
   }
   else
   {
      return NULL;
   }   
}

static jintArray com_android_nfc_NativeNfcSecureElement_doGetTechList(JNIEnv *e, jobject o, jint handle)
{
   jintArray techList;
   LOGD("Get Secure element Type function ");
      
   if(handle == secureElementHandle)
   {
      techList = e->NewIntArray(1);
      e->SetIntArrayRegion(techList, 0, 1, &SecureElementTech);
      return techList;
   }
   else
   {
      return NULL;
   }
}


/*
 * JNI registration.
 */
static JNINativeMethod gMethods[] =
{
   {"doOpenSecureElementConnection", "()I",
      (void *)com_android_nfc_NativeNfcSecureElement_doOpenSecureElementConnection},
   {"doDisconnect", "(I)Z",
      (void *)com_android_nfc_NativeNfcSecureElement_doDisconnect},
   {"doTransceive", "(I[B)[B",
      (void *)com_android_nfc_NativeNfcSecureElement_doTransceive},
   {"doGetUid", "(I)[B",
      (void *)com_android_nfc_NativeNfcSecureElement_doGetUid},
   {"doGetTechList", "(I)[I",
      (void *)com_android_nfc_NativeNfcSecureElement_doGetTechList},
};

int register_com_android_nfc_NativeNfcSecureElement(JNIEnv *e)
{
   return jniRegisterNativeMethods(e,
      "com/android/nfc/NativeNfcSecureElement",
      gMethods, NELEM(gMethods));
}

} // namespace android
