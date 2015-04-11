/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStatusBarBiffManager.h"
#include "nsMsgBiffManager.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsIMsgMailSession.h"
#include "MailNewsTypes.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsIFileChannel.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#include "nsIFile.h"
#include "nsMsgUtils.h"
#include "mozilla/Services.h"

// QueryInterface, AddRef, and Release
//
NS_IMPL_ISUPPORTS3(nsStatusBarBiffManager, nsIStatusBarBiffManager, nsIFolderListener, nsIObserver)

nsIAtom * nsStatusBarBiffManager::kBiffStateAtom = nullptr;

nsStatusBarBiffManager::nsStatusBarBiffManager()
: mInitialized(false), mCurrentBiffState(nsIMsgFolder::nsMsgBiffState_Unknown)
{
}

nsStatusBarBiffManager::~nsStatusBarBiffManager()
{
    NS_IF_RELEASE(kBiffStateAtom);
}

#define PREF_PLAY_SOUND_ON_NEW_MAIL      "mail.biff.play_sound"
#define PREF_NEW_MAIL_SOUND_URL          "mail.biff.play_sound.url"
#define PREF_NEW_MAIL_SOUND_TYPE         "mail.biff.play_sound.type"
#define SYSTEM_SOUND_TYPE 0
#define CUSTOM_SOUND_TYPE 1
#define PREF_CHAT_ENABLED                "mail.chat.enabled"
#define PREF_CHAT_PLAY_SOUND             "mail.chat.play_notification_sound"
#define NEW_CHAT_MESSAGE_TOPIC           "new-directed-incoming-message"

nsresult nsStatusBarBiffManager::Init()
{
  if (mInitialized)
    return NS_ERROR_ALREADY_INITIALIZED;

  nsresult rv;

  kBiffStateAtom = MsgNewAtom("BiffState");

  nsCOMPtr<nsIMsgMailSession> mailSession = 
    do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 
  if(NS_SUCCEEDED(rv))
    mailSession->AddFolderListener(this, nsIFolderListener::intPropertyChanged);

  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool chatEnabled = false;
  if (NS_SUCCEEDED(rv))
    rv = pref->GetBoolPref(PREF_CHAT_ENABLED, &chatEnabled);
  if (NS_SUCCEEDED(rv) && chatEnabled) {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService)
      observerService->AddObserver(this, NEW_CHAT_MESSAGE_TOPIC, false);
  }

  mInitialized = true;
  return NS_OK;
}

nsresult nsStatusBarBiffManager::PlayBiffSound()
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv,rv);
  

  // lazily create the sound instance
  if (!mSound)
    mSound = do_CreateInstance("@mozilla.org/sound;1");
      
  int32_t newMailSoundType = SYSTEM_SOUND_TYPE;
  rv = pref->GetIntPref(PREF_NEW_MAIL_SOUND_TYPE, &newMailSoundType);
  NS_ENSURE_SUCCESS(rv,rv);

  bool customSoundPlayed = false;

  if (newMailSoundType == CUSTOM_SOUND_TYPE) {
    nsCString soundURLSpec;
    rv = pref->GetCharPref(PREF_NEW_MAIL_SOUND_URL, getter_Copies(soundURLSpec));
    if (NS_SUCCEEDED(rv) && !soundURLSpec.IsEmpty()) {
      if (!strncmp(soundURLSpec.get(), "file://", 7)) {
        nsCOMPtr<nsIURI> fileURI;
        rv = NS_NewURI(getter_AddRefs(fileURI), soundURLSpec);
        NS_ENSURE_SUCCESS(rv,rv);
        nsCOMPtr<nsIFileURL> soundURL = do_QueryInterface(fileURI,&rv);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIFile> soundFile;
          rv = soundURL->GetFile(getter_AddRefs(soundFile));
          if (NS_SUCCEEDED(rv)) {
            bool soundFileExists = false;
            rv = soundFile->Exists(&soundFileExists);
            if (NS_SUCCEEDED(rv) && soundFileExists) {
              rv = mSound->Play(soundURL);
              if (NS_SUCCEEDED(rv))
                customSoundPlayed = true;
            }
          }
        }
      }
      else {
        // todo, see if we can create a nsIFile using the string as a native path.
        // if that fails, try playing a system sound
        NS_ConvertUTF8toUTF16 utf16SoundURLSpec(soundURLSpec);
        rv = mSound->PlaySystemSound(utf16SoundURLSpec);
        if (NS_SUCCEEDED(rv))
          customSoundPlayed = true;
      }
    }
  }    
  
  // if nothing played, play the default system sound
  if (!customSoundPlayed) {
#ifdef XP_MACOSX
    // Mac has no specific event sounds, so just beep instead.
    rv = mSound->Beep();
#else
    rv = mSound->PlayEventSound(nsISound::EVENT_NEW_MAIL_RECEIVED);
#endif
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return rv;
}

// nsIFolderListener methods....
NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemAdded(nsIMsgFolder *parentItem, nsISupports *item)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemRemoved(nsIMsgFolder *parentItem, nsISupports *item)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemPropertyChanged(nsIMsgFolder *item, nsIAtom *property, const char *oldValue, const char *newValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsStatusBarBiffManager::OnItemIntPropertyChanged(nsIMsgFolder *item, nsIAtom *property, int32_t oldValue, int32_t newValue)
{
  if (kBiffStateAtom == property && mCurrentBiffState != newValue) {
    // if we got new mail, attempt to play a sound.
    // if we fail along the way, don't return.
    // we still need to update the UI.    
    if (newValue == nsIMsgFolder::nsMsgBiffState_NewMail) {
      nsresult rv;
      nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      bool playSoundOnBiff = false;
      rv = pref->GetBoolPref(PREF_PLAY_SOUND_ON_NEW_MAIL, &playSoundOnBiff);
      NS_ENSURE_SUCCESS(rv, rv);
      if (playSoundOnBiff) {
        // if we fail to play the biff sound, keep going.
        (void)PlayBiffSound();
      }
    }
    mCurrentBiffState = newValue;

    // don't care if notification fails
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
      
    if (observerService)
      observerService->NotifyObservers(static_cast<nsIStatusBarBiffManager*>(this), "mail:biff-state-changed", nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemBoolPropertyChanged(nsIMsgFolder *item, nsIAtom *property, bool oldValue, bool newValue)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemUnicharPropertyChanged(nsIMsgFolder *item, nsIAtom *property, const PRUnichar *oldValue, const PRUnichar *newValue)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemPropertyFlagChanged(nsIMsgDBHdr *item, nsIAtom *property, uint32_t oldFlag, uint32_t newFlag)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemEvent(nsIMsgFolder *item, nsIAtom *event)
{
  return NS_OK;
}

// nsIObserver implementation
NS_IMETHODIMP
nsStatusBarBiffManager::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const PRUnichar *aData)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  bool playSound = false;
  rv = pref->GetBoolPref(PREF_CHAT_PLAY_SOUND, &playSound);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!playSound)
    return NS_OK;

  return PlayBiffSound();
}

// nsIStatusBarBiffManager method....
NS_IMETHODIMP
nsStatusBarBiffManager::GetBiffState(int32_t *aBiffState)
{
  NS_ENSURE_ARG_POINTER(aBiffState);
  *aBiffState = mCurrentBiffState;
  return NS_OK;
}
 
