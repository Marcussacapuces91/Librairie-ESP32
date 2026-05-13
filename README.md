# Librairie-ESP32

Quelques fichiers pour me faciliter le travail.

## Wifi Manager

**Prompt :**

> Génère une classe C++ `WifiManager` pour ESP-IDF, en respectant strictement les contraintes suivantes :
>
> ### 1. Architecture générale
> - La classe doit être **RAII** :  
>   - le constructeur initialise tout (esp_netif_init, esp_wifi_init, interface STA, handlers, sémaphore)  
>   - le destructeur nettoie tout (unregister handlers, esp_wifi_stop, esp_wifi_deinit, delete semaphore)
> - Pas de global, pas de singleton, pas de variable statique d’instance.
> - Le handler ESP-IDF doit être **unique** :  
>   `static void eventHandler(void* arg, esp_event_base_t, int32_t, void*)`  
>   Il doit redispatcher vers des méthodes internes.
> - Les méthodes internes doivent être :  
>   `onStaStart`, `onStaDisconnected`, `onGotSsidPass`, `onGotIp`, `startSmartConfig`.
> - Ces méthodes doivent être **protected** pour permettre la surcharge.
>
> ### 2. Fonctionnalités
> - `connect()` démarre le WiFi, attend l’IP via un sémaphore, puis retourne `esp_netif_t*`.
> - Gestion du retry automatique en cas de déconnexion (3 essais).
> - Si plus de retry → lancer SmartConfig.
> - SmartConfig doit appliquer les credentials, arrêter SmartConfig, reconnecter.
> - `clearCredentials()` doit :
>   - appeler `esp_wifi_stop()`  
>   - ignorer `ESP_ERR_WIFI_NOT_STARTED`  
>   - appeler `esp_wifi_restore()`  
>   - redémarrer le WiFi si nécessaire  
>   - être **safe même avant connect()**
>
> ### 3. Documentation
> - Ajouter un **header Doxygen complet** pour la classe :
>   - description  
>   - RAII  
>   - SmartConfig  
>   - exemple d’usage
> - Ajouter un header Doxygen **pour chaque méthode publique et protected**.
> - Ajouter une section dans la doc expliquant que les méthodes protected sont conçues pour être **surchargées** dans une classe dérivée, avec un exemple d’override.
>
> ### 4. Style
> - Code moderne, clair, propre, idiomatique.
> - Pas de macros inutiles.
> - Pas de code mort.
> - Logs ESP_LOGI/W/E.
> - Pas de modification de la logique demandée.
>
> ### 5. Sortie attendue
> - Fournir **un seul bloc de code complet**, header + implémentation dans le même fichier.
> - Le code doit être directement compilable sous ESP-IDF.
>
> Génère maintenant la classe complète.
