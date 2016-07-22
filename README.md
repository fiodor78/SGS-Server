SGS API
=============
-------------


**Uwaga!**
Zmiany w SGS API to prawie zawsze 'breaking change' - powodują konieczność przekompilowania razem serwera gry i logiki.

Changelog:

* **1.11** (2014/09/10)
    * Dodanie funkcji `ILogicUpCalls::PerformFireAndForgetHTTPRequest()`.
	
* **1.10** (2014/06/17)
    * Dodanie funkcji `ILogicUpCalls::RegisterBuddies()`.
    * Dodanie funkcji `ILogicUpCalls::GetGamestateIDFromLocationID()`.
    * Dodanie funkcji `ILogicUpCalls::GetLocationIDFromGamestateID()`.
    * Dodanie funkcji `ILogicUpCalls::SendGlobalMessage()`.
    * Dodanie funkcji `ILogicUpCalls::GetLocationsIdsLike()`.
    * Dodanie funkcji `ILogicUpCalls::ReplaceVulgarWords()`.
    * Dodanie callbacka `ILogicEngineBase::on_get_locations_ids_like()`.
    * Dodanie callbacka `ILogicEngineBase::on_replace_vulgar_words()`.
    * Dodanie callbacka `ILogicEngineBase::handle_system_global_message()`.

* **1.09** (2014/01/20)
    * Dodanie callbacka `ILogicEngineBase::handle_system_location_message()`.

* **1.08** (2014/01/16)
    * Dodanie callbacka `ILogicEngine::handle_global_message()`.


* **1.07** (2013/11/14)
    * Dodanie funkcji `ILogicUpCalls::PerformHTTPRequest()`.
    * Dodanie callbacka `ILogicEngine::on_http_response()`.


* **1.06** (2013/10/02)
    * Dodanie callbacka `ILogicEngine::on_connect_player()`.


* **1.05** (2013/04/12)
    * Dodanie callbacka `ILogicEngineBase::on_clock_tick()`.


* **1.04** (2013/02/26)
    * Dodanie funkcji `ILogicUpCalls::SubmitAnalyticsEvent()`.


* **1.03** (2012/12/04)
    * Dodanie funkcji `ILogicUpCalls::RequestLeaderboardUserPosition()`.


* **1.02** (2012/10/02)
    * Zmiana kolejności funkcji w interfejsie `ILogicUpCalls`.


* **1.01** (2012/10/02)
    * Pierwsza wersjonowana wersja SGS API.
