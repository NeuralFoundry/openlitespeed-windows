[English](../../README.md) | [Türkçe](README.tr.md) | [简体中文](README.zh-CN.md) | [Español](README.es.md) | [Português](README.pt-BR.md) | [Русский](README.ru.md) | [日本語](README.ja.md) | [Deutsch](README.de.md) | **Français** | [한국어](README.ko.md) | [Bahasa Indonesia](README.id.md)

# OpenLiteSpeed for Windows

Site web : [openlitespeed.dev/windows](https://openlitespeed.dev/windows/)

OpenLiteSpeed pour Windows est une version native pour Windows du serveur web [OpenLiteSpeed](https://openlitespeed.org) 1.9.4. Il fonctionne sous Windows 10, Windows 11 et Windows Server sans WSL, Docker ni machine virtuelle, exécute PHP via LSAPI ou FastCGI et s'installe seul ou à l'intérieur de Laragon et XAMPP. L'édition full stack ajoute un tableau de bord, MariaDB, phpMyAdmin et Cloudflare Tunnel, de sorte qu'un site sur votre ordinateur peut être mis en ligne même derrière une IP locale. Depuis le tableau de bord, vous installez des versions de PHP et de Node.js, donnez à chaque site sa propre version de PHP, créez des sites WordPress, Laravel, Node.js et Python et gérez les bases de données. Gratuit et open source sous licence GPL-3.0.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-dark.png">
  <img src="../panel-light.png" width="880" alt="Vue d'ensemble du tableau de bord d'OpenLiteSpeed pour Windows avec le serveur web, MariaDB, Cloudflare Tunnel, le nombre de requêtes en direct et les sites locaux">
</picture>

## Fonctionnalités

- Programme Win32 natif compilé avec Visual C++, avec un installateur graphique, une application dans la zone de notification et un service Windows
- Deux éditions dans un seul installateur : le serveur web seul, ou le full stack avec tableau de bord, MariaDB, phpMyAdmin et Cloudflare Tunnel
- Tableau de bord : une seule fenêtre pour démarrer et arrêter les services, créer des sites, installer PHP et Node.js, gérer les bases de données, lire les journaux, changer les ports et mettre des sites en ligne
- PHP 7.4 à 8.5 et versions LTS de Node.js installés depuis le tableau de bord et vérifiés avec leur SHA-256 publié
- Une version de PHP par site, des extensions PHP activées et désactivées par un interrupteur et les réglages courants de `php.ini` dans un formulaire
- Nouveaux sites : site PHP simple, WordPress, Laravel (avec Composer), application Node.js ou application Python
- Bases de données : créer une base avec son propre utilisateur, exporter et importer des fichiers `.sql`, supprimer une base
- Mettre un site local en ligne avec Cloudflare Tunnel : une adresse https publique pour un site sur votre ordinateur, sans redirection de port ni IP publique. Votre propre domaine fonctionne avec un jeton de tunnel.
- MariaDB 11.8 et phpMyAdmin 5.2 uniquement sur 127.0.0.1, avec un mot de passe root généré. Chaque nouveau site WordPress reçoit sa propre base de données et son propre utilisateur.
- Utilise tous les cœurs du processeur : jusqu'à 32 processus worker HTTP sur les mêmes ports, redémarrés automatiquement après un plantage, avec rechargement sans interruption de service
- PHP 8.2, 8.3, 8.4 et 8.5 via LSAPI (`lsphp.exe` pour Windows est inclus) ou FastCGI (`php-cgi.exe`)
- Détecte les versions de PHP installées, Laragon et XAMPP et se configure en conséquence
- Importe les sites Apache depuis `httpd.conf` : hôtes virtuels, certificats SSL, redirections, alias, proxys, CGI, règles d'accès et protection par mot de passe, y compris les règles des fichiers `.htaccess`
- Programmes CGI : fichiers `.exe`, `.bat` et `.cmd` et scripts avec une ligne `#!` comme Perl ou Python, chacun dans son propre objet de traitement (job object) avec un jeton restreint
- Applications Node.js et Python (WSGI), démarrées et redémarrées par le serveur. Les applications Node.js fonctionnent sans modification grâce à [openlitespeed-node](https://github.com/NeuralFoundry/openlitespeed-node), WebSockets compris, voir [docs/APPS.md](../APPS.md)
- Sites de développement local : chaque dossier du dossier des projets devient `http://folder.test/`. Nouveaux sites, avec WordPress si vous le souhaitez, depuis la zone de notification ou le tableau de bord
- HTTPS de confiance pour les sites locaux : une autorité de certification créée sur votre ordinateur, qui ne peut signer que les noms `.test`, `.localhost` et `.internal` et l'adresse propre de l'ordinateur
- Sites proxy pour les applications qui tournent sur un port local, par exemple `api.test` pour une application sur `127.0.0.1:3000`
- Mises à jour signées : l'application de la zone de notification télécharge une nouvelle version, vérifie sa signature et l'installe quand vous confirmez
- HTTP/2, HTTP/3 (QUIC), TLS 1.3, Brotli, LSCache et la console WebAdmin
- Certificats HTTPS gratuits de Let's Encrypt, demandés et renouvelés automatiquement par un client ACME intégré, voir [docs/ACME.md](../ACME.md)
- Modules intégrés : LSCache, progression des envois, analyse du corps des requêtes et scripts Lua avec LuaJIT, voir [docs/MODULES.md](../MODULES.md)
- Les fichiers `.htaccess` fonctionnent comme sous Apache : règles de réécriture, règles d'accès (`Require`, `Order`, `Allow`, `Deny`), mots de passe, sections `<Files>`, `Options Indexes` et `php_flag engine off`, appliqués en moins d'une seconde quand un fichier change
- Pare-feu applicatif web : ModSecurity 3 avec l'OWASP Core Rule Set, activé en une seule commande, voir [docs/PROTECTION.md](../PROTECTION.md#web-application-firewall)
- Protection contre les bots : défis reCAPTCHA ou hCaptcha, limites de requêtes, de bande passante et de connexions par IP client, et pays du visiteur à partir des bases MaxMind GeoIP2 ou IP2Location pour PHP et les règles de réécriture, voir [docs/PROTECTION.md](../PROTECTION.md)
- LiteSpeed Cache pour WordPress fonctionne immédiatement : installez et activez l'extension, aucune configuration du serveur n'est nécessaire. Les pages sont purgées automatiquement quand le contenu change.
- Renforcé pour Windows : les noms de fichiers sont vérifiés contre les astuces de casse, de noms courts, de flux de données et de noms de périphériques, PHP s'exécute avec un jeton restreint et le service sous un compte virtuel

## Performances : OpenLiteSpeed pour Windows face à nginx et Apache

La version native pour Windows de ce dépôt face à nginx et Apache sur la même machine Windows 11, avec PHP 8.4 et les mêmes 12 cœurs de processeur pour chaque serveur. Requêtes par seconde, plus c'est élevé, mieux c'est. Tous les tests se sont terminés sans erreur.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-overview-dark.svg">
  <img src="../bench-windows-overview-light.svg" width="880" alt="OpenLiteSpeed pour Windows face à nginx et Apache : requêtes par seconde pour un fichier statique, une application PHP et WordPress">
</picture>

| Serveur | Fichier statique | Application PHP | WordPress, sans cache | PHP en HTTPS | PHP à 512 connexions |
|---|--:|--:|--:|--:|--:|
| OpenLiteSpeed + LSAPI | 175 203 | 9 548 | 117 | 8 687 | 9 434 |
| OpenLiteSpeed + FastCGI |  | 9 538 | 119 | 8 961 | 9 587 |
| nginx + PHP-CGI | 60 306 | 2 949 | 122 | 472 | 462 |
| Apache + mod_php | 17 889 | 7 532 | 105 | 4 473 | 3 078 |
| Apache + PHP-CGI |  | 5 743 | 118 | 4 356 | 5 396 |

Les fichiers statiques ne passent pas par PHP, ils ont donc été mesurés une seule fois par serveur. Fichier statique, application PHP et WordPress : tests de 20 secondes avec 128 connexions keep-alive, 64 pour WordPress. HTTPS : 10 secondes, 128 connexions. La dernière colonne est la moyenne d'un test de charge continu de 60 secondes avec 512 connexions :

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../bench-windows-stress-app-dark.svg">
  <img src="../bench-windows-stress-app-light.svg" width="880" alt="Test de charge d'OpenLiteSpeed pour Windows : requêtes par seconde toutes les 10 secondes pendant 60 secondes avec 512 connexions">
</picture>

WordPress sans cache tourne à peu près à la même vitesse sur tous les serveurs, car PHP et MySQL font l'essentiel du travail. Avec un cache de pages, la situation change. La page d'accueil du même site, 64 connexions :

| Serveur et cache | Requêtes/s | p99 |
|---|--:|--:|
| OpenLiteSpeed + extension LiteSpeed Cache | 21 180 | 6,1 ms |
| Apache + mod_cache_disk | 18 362 | 6,1 ms |
| nginx + fastcgi_cache | 7 745 | 10,8 ms |

La latence, la mémoire, la configuration complète et les données brutes se trouvent dans [docs/BENCHMARKS.md](../BENCHMARKS.md).

## Configuration requise

- Windows 10, Windows 11 ou Windows Server 2019 ou version ultérieure, x64
- PHP 8.2 ou version ultérieure pour Windows, x64. L'installateur peut le télécharger.

## Installation

Téléchargez `OpenLiteSpeed-1.9.4-win64-setup.exe` depuis [Releases](https://github.com/NeuralFoundry/openlitespeed-windows/releases/latest) et exécutez-le. L'installateur affiche les versions de PHP trouvées, propose le type d'installation et demande l'édition.

<img src="../setup.png" width="576" alt="Installateur d'OpenLiteSpeed pour Windows avec le choix de l'édition et la version de PHP">

| Édition | Contenu |
|---|---|
| Full stack | OpenLiteSpeed, PHP, l'application de la zone de notification, le tableau de bord, MariaDB, phpMyAdmin et Cloudflare Tunnel. L'installateur télécharge environ 170 Mo et vérifie chaque fichier avec un SHA-256 figé. |
| Serveur web seul | OpenLiteSpeed, PHP et l'application de la zone de notification, avec nouveaux sites, HTTPS de confiance, changement de version de PHP, imports et mises à jour |

Relancer l'installateur permet de changer d'édition. Le passage à l'édition serveur web seul conserve vos bases de données. L'édition full stack fonctionne avec l'application de la zone de notification, l'option de service Windows est donc réservée à l'édition serveur web seul.

L'installateur limite l'accès au dossier d'installation à votre compte, aux administrateurs et au système, afin que les autres utilisateurs de l'ordinateur ne puissent pas modifier les programmes ni la configuration. Il ne modifie pas les dossiers de Laragon et XAMPP, dont les dossiers PHP peuvent souvent être modifiés par d'autres utilisateurs ; avec le service Windows, cela leur permet d'exécuter du code sous le compte du service, c'est pourquoi l'installation du service avertit au sujet de ces dossiers PHP.

| Type d'installation | Dossier d'installation | Dossier des projets | Démarré par |
|---|---|---|---|
| Serveur autonome | `C:\OpenLiteSpeed` | `C:\OpenLiteSpeed\www` | Application de la zone de notification ou service Windows |
| Dans Laragon | `C:\laragon\bin\openlitespeed` | Racine des documents de Laragon | Laragon **Start All** |
| Dans XAMPP | `C:\xampp\openlitespeed` | `C:\xampp\htdocs` | Application de la zone de notification ou service Windows |

Le site web répond sur le port 80, ou sur le port 8088 si le port 80 est occupé. WebAdmin se trouve à l'adresse `https://localhost:7080/` avec l'utilisateur `admin` et le mot de passe affiché à la fin de l'installation. Relancer l'installateur sur une installation existante la met à niveau en conservant la configuration.

<img src="../webadmin.png" width="880" alt="Console WebAdmin d'OpenLiteSpeed sous Windows">

Installation sans surveillance :

```bat
OpenLiteSpeed-1.9.4-win64-setup.exe --silent --mode standalone --download-php 8.4 --edition full
```

| Option | Description |
|---|---|
| `--mode standalone\|laragon\|xampp` | Type d'installation, détecté s'il est omis |
| `--edition server\|full` | Édition ; si elle est omise, `server` pour une nouvelle installation et l'édition actuelle pour une mise à niveau. `--service` nécessite `server` |
| `--php <folder>` / `--download-php 8.2\|8.3\|8.4\|8.5` | PHP à utiliser |
| `--dir <folder>` / `--www <folder>` | Dossier d'installation / dossier des projets |
| `--port <n>` / `--admin-port <n>` | Ports du site web et de WebAdmin |
| `--service` / `--no-tray` | Installer le service Windows / ne pas démarrer l'application de la zone de notification |
| `--log <file>` | Fichier journal, `%TEMP%\OpenLiteSpeed-setup.log` par défaut, contient le mot de passe administrateur |

Les mêmes fichiers sont disponibles dans un zip avec `install.ps1` pour les déploiements par script. Comme l'installateur, il limite l'accès au dossier d'installation à votre compte, aux administrateurs et au système lorsque le dossier hérite encore ses autorisations du dossier parent :

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -Mode standalone -DownloadPhp 8.4 -Edition full
```

Pour désinstaller, utilisez **Paramètres > Applications** ou exécutez `uninstall.exe --uninstall --silent` dans le dossier d'installation. Le dossier des projets et le dossier `data` contenant les bases de données sont conservés.

## Tableau de bord

Dans l'édition full stack, le tableau de bord s'ouvre depuis le menu Démarrer ou par un double clic sur l'icône de la zone de notification. C'est une fenêtre de bureau, pas une page web, il n'écoute donc sur aucun port. Les pages :

| Page | Ce que vous pouvez faire |
|---|---|
| Overview | Démarrer et arrêter le serveur web, MariaDB et Cloudflare Tunnel, voir les requêtes par seconde et les connexions ouvertes |
| Sites | Créer et supprimer des sites, choisir la version de PHP ou de Node.js de chaque site, mettre un site en ligne, ouvrir son dossier, un terminal ou VS Code dans ce dossier, exécuter un dossier comme application Node.js ou Python, exécuter des tâches en arrière-plan |
| PHP | Installer des versions de PHP, choisir celle par défaut, activer et désactiver les extensions et Xdebug, modifier les réglages de `php.ini`, installer Composer |
| Node.js | Installer des versions de Node.js, choisir celle qui exécute les applications, redémarrer une application et lire son journal |
| Databases | Créer une base de données et son utilisateur, exporter et importer des fichiers `.sql`, supprimer une base, ouvrir phpMyAdmin |
| Mail | Activer et désactiver l'intercepteur d'e-mails, lire les e-mails interceptés avec leur partie HTML et leurs pièces jointes |
| Logs | Lire les journaux du serveur, de MariaDB, des applications et du tunnel, filtrer les lignes, suivre les nouvelles lignes |
| Settings | Démarrage avec Windows, le dossier des projets, HTTPS de confiance, les ports HTTP et WebAdmin et le mot de passe WebAdmin |

**Terminal**, en bas à gauche, ouvre une invite de commandes avec le PHP par défaut, Composer, Node.js et le client MariaDB dans le `PATH`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-sites-dark.png">
  <img src="../panel-sites-light.png" width="880" alt="Page Sites du tableau de bord avec une version de PHP par site, des applications Node.js et Python, un site proxy et les interrupteurs Online">
</picture>

Le tableau de bord nécessite Microsoft Edge WebView2 Runtime, fourni avec Windows 11 et les versions actuelles de Windows 10.

## Sites locaux

Chaque dossier du dossier des projets est un site à l'adresse `http://folder.test/`. **New site** dans le tableau de bord ou la zone de notification crée le dossier. Le tableau de bord propose ces types de sites :

| Type | Ce que vous obtenez |
|---|---|
| Site PHP | Un dossier avec un `index.php` |
| WordPress | La dernière version de WordPress. Si MariaDB est installé, une base de données et un utilisateur pour le site et un `wp-config.php` déjà rempli, de sorte que l'installation de WordPress ne demande que le titre du site et un compte administrateur |
| Laravel | Un nouveau projet Laravel créé par `composer create-project`, servi depuis son dossier `public` |
| Application Node.js | Un petit `server.js` et un `package.json`, démarrés par le serveur avec le Node.js par défaut |
| Application Python | Un `wsgi.py` WSGI, démarré par le serveur avec le Python trouvé sur l'ordinateur |

L'installateur de WordPress ne s'ouvre que depuis cet ordinateur, si bien que personne d'autre sur le réseau ne peut le terminer avant vous. Un dossier contenant un `package.json` devient une application Node.js avec **More > Run as a Node.js app**, et **Run as a PHP site** revient en arrière. **Remove site** déplace le dossier dans la Corbeille et conserve les bases de données.

**New proxy site** donne un nom propre à une application qui écoute sur un port local, par exemple `app.test` pour un serveur de développement Vite sur le port 5173. Les WebSockets sont transmis. Un site proxy ne répond qu'à cet ordinateur, comme l'application qui se trouve derrière.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-site-dark.png">
  <img src="../panel-site-light.png" width="420" alt="Boîte de dialogue New site avec le nom du site et les types de sites : PHP, WordPress, Laravel, application Node.js et application Python">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-proxy-dark.png">
  <img src="../panel-proxy-light.png" width="420" alt="Boîte de dialogue New proxy site avec le nom du site et le port de l'application">
</picture>

## Mettre un site en ligne

Activez **Online** à côté d'un site dans le tableau de bord ou exécutez `openlitespeed --tunnel start <site>`. Cloudflare attribue au site une adresse aléatoire `https://*.trycloudflare.com` que tout le monde peut ouvrir jusqu'à ce que vous la désactiviez. Cela fonctionne derrière un NAT, un routeur ou une IP locale, car la connexion part de votre ordinateur, et rien n'est ouvert dans le pare-feu. Les visiteurs conservent leur propre adresse IP dans les journaux et dans PHP. L'adresse publique s'affiche sous l'adresse locale, avec **Copy** à côté.

Pour votre propre domaine, créez un tunnel dans Cloudflare Zero Trust, faites pointer son nom d'hôte public vers `http://127.0.0.1` sur le port du site web avec le nom du site comme en-tête HTTP Host, puis collez le jeton du tunnel dans le tableau de bord sous **Use your own domain**. Le jeton est stocké chiffré pour votre compte Windows.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-share-dark.png">
  <img src="../panel-share-light.png" width="420" alt="Boîte de dialogue pour partager un site local en ligne via Cloudflare">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-token-dark.png">
  <img src="../panel-token-light.png" width="420" alt="Boîte de dialogue pour connecter votre propre domaine avec un jeton de tunnel Cloudflare">
</picture>

Seuls les sites du dossier des projets et les sites proxy peuvent être mis en ligne. Le partage d'un site proxy connecte le tunnel directement à son application ; avec votre propre domaine, faites aussi pointer ce nom d'hôte vers le port de l'application. phpMyAdmin et WebAdmin ne répondent qu'à cet ordinateur, et un tunnel ne démarre pas si le serveur ne peut pas distinguer ses visiteurs de cet ordinateur.

## Bases de données

MariaDB écoute uniquement sur `127.0.0.1` et s'exécute sous votre compte, jamais en tant qu'administrateur. L'installateur le télécharge, et l'application de la zone de notification crée les bases de données à son premier démarrage. Le mot de passe root est généré à ce moment et stocké dans `data\client.cnf`, lisible uniquement par votre compte et les administrateurs. `SELECT ... INTO OUTFILE` et `LOAD DATA` ne fonctionnent que dans `data\files`. **Copy password** dans le tableau de bord ou la zone de notification place le mot de passe dans le Presse-papiers sans l'ajouter à l'historique du Presse-papiers. phpMyAdmin se trouve à l'adresse `http://phpmyadmin.test/`, pour cet ordinateur uniquement.

Un site WordPress créé depuis la zone de notification ou le tableau de bord reçoit sa propre base de données et son propre utilisateur, et `wp-config.php` est rempli.

Sur la page **Databases**, **New database** crée une base de données avec un utilisateur du même nom et un mot de passe généré, affiché une seule fois avec un bouton **Copy**. **Export** écrit un fichier `.sql` avec `mariadb-dump`, **Import** charge un fichier `.sql` dans la base de données, et **Remove** vous demande d'abord de saisir le nom et peut aussi supprimer l'utilisateur.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-db-dark.png">
  <img src="../panel-db-light.png" width="880" alt="Page Databases du tableau de bord avec MariaDB, le mot de passe root et l'export, l'import et la suppression pour chaque base de données">
</picture>

## HTTPS de confiance

**Trusted HTTPS** dans le tableau de bord ou la zone de notification, ou `openlitespeed --dev-https on`, crée une autorité de certification sur votre ordinateur, demande à Windows de lui faire confiance pour votre compte et sert chaque site local sur `https://name.test/`. L'autorité est limitée par des contraintes de nom (name constraints) aux noms `.test`, `.localhost` et `.internal` et à `127.0.0.1` et `::1`, elle ne peut donc signer de certificat pour aucun autre site. La désactiver la retire du magasin de certificats de confiance de Windows.

## PHP

`lsphp.exe` est la SAPI PHP LiteSpeed compilée pour Windows, pour PHP 8.2 à 8.5 en variantes thread safe et non thread safe. Elle s'exécute au-dessus des versions officielles de PHP publiées sur [windows.php.net](https://windows.php.net), si bien que les extensions existantes et les réglages de `php.ini` continuent de fonctionner. Si une version de PHP ne peut pas la charger, OpenLiteSpeed se rabat sur `php-cgi.exe` via FastCGI.

<img src="../phpinfo.png" width="640" alt="phpinfo de PHP 8.4 sous Windows avec Server API LiteSpeed">

Changez de version de PHP depuis le tableau de bord, le menu de la zone de notification ou avec `openlitespeed --set-php <folder>`.

**Install PHP** sur la page PHP liste les versions de windows.php.net, de 7.4 à 8.5, thread safe et non thread safe. Un téléchargement est vérifié avec le SHA-256 publié par windows.php.net, décompressé dans `php\<version>` et reçoit un `php.ini` avec les extensions habituelles activées. Les versions à partir de 8.2 passent par LSAPI, les plus anciennes par FastCGI.

Chaque site peut utiliser sa propre version de PHP : choisissez-la dans la colonne PHP de la page Sites, ou exécutez `openlitespeed --site php <site> <folder>`. Les sites sans choix suivent la version par défaut. Pour chaque version, le menu **More** contient **Extensions**, avec un interrupteur pour chaque extension de ce PHP, et **Settings** pour la limite de mémoire, la taille des envois et des requêtes POST, le temps d'exécution, les variables d'entrée, l'affichage des erreurs et le fuseau horaire. **Edit php.ini** ouvre le fichier dans le Bloc-notes. Les modifications rechargent le serveur web.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-php-dark.png">
  <img src="../panel-php-light.png" width="880" alt="Page PHP du tableau de bord avec trois versions de PHP, la version par défaut, les sites qui utilisent chaque version et Composer">
</picture>

**Install** sous Composer télécharge `composer.phar` depuis getcomposer.org et le vérifie avec son SHA-256. Le terminal du tableau de bord a `composer` dans le `PATH`, et les sites Laravel sont créés avec.

## Xdebug

L'interrupteur de la colonne Xdebug de la page PHP, ou `openlitespeed --php xdebug <folder> on`, active Xdebug pour cette version de PHP. Si un Xdebug est déjà chargé depuis le dossier `ext`, il est utilisé. Sinon, la version officielle pour Windows correspondant à cette version de PHP est téléchargée depuis xdebug.org et vérifiée avec un SHA-256 conservé dans OpenLiteSpeed : Xdebug 3.5.3 pour PHP 8.2 à 8.5, thread safe et non thread safe.

Xdebug démarre à chaque requête et se connecte à un débogueur sur `127.0.0.1:9003`. Sans débogueur, la requête continue aussitôt et rien n'est écrit dans les journaux. Le JIT d'opcache est désactivé tant que Xdebug est actif et revient quand Xdebug est désactivé. Les réglages sont conservés à la fin de `php.ini`, et le serveur web est rechargé pour les appliquer.

Xdebug ne peut pas être activé tant qu'OpenLiteSpeed est installé comme service Windows. N'importe quel programme de l'ordinateur qui écoute sur le port 9003 reçoit la session de débogage et peut exécuter du code PHP sous le compte du service, désactivez donc Xdebug avant d'installer le service.

**VS Code :** **More > Open in VS Code** sur la page Sites ouvre le dossier du site dans VS Code s'il est installé. Lancez une session de débogage PHP qui écoute sur le port 9003, par exemple avec l'extension PHP Debug, et ouvrez le site dans le navigateur.

## Intercepteur d'e-mails

L'interrupteur de la page Mail, ou `openlitespeed --mail on`, intercepte tous les e-mails envoyés par les sites locaux, si bien que rien ne quitte l'ordinateur. La fonction PHP `mail()` passe par OpenLiteSpeed via `sendmail_path` dans le `php.ini` des versions de PHP qu'OpenLiteSpeed exécute, et les applications qui envoient en SMTP utilisent l'hôte `127.0.0.1`, le port 1025, sans chiffrement et avec n'importe quel nom d'utilisateur et mot de passe. Les nouveaux sites Laravel reçoivent ces réglages dans `.env`. Une connexion qui commence comme une requête web est fermée, si bien qu'une page web ouverte dans le navigateur ne peut pas envoyer d'e-mail à l'intercepteur.

Les e-mails sont conservés dans `data\mail`, lisible uniquement par votre compte, dans la limite des 1000 messages les plus récents et de 512 Mo. La page Mail affiche chaque message avec ses parties texte et HTML et permet d'enregistrer les pièces jointes. La partie HTML s'affiche sans scripts et ne charge rien depuis Internet.

## Tâches en arrière-plan

**Background jobs** dans le menu **More** d'un site, ou `openlitespeed --job add <site> <name> <command>`, exécute une commande de longue durée comme `php artisan queue:work`, `php artisan schedule:work` ou `npm run dev` dans le dossier du site, avec les versions de PHP et de Node.js de ce site en tête du `PATH`. Les tâches démarrent et s'arrêtent avec le serveur web. Une tâche qui se termine est relancée après 1, 2, 4 et jusqu'à 60 secondes, et après 10 redémarrages en 10 minutes, elle reste arrêtée. Arrêter une tâche met fin à tous les processus qu'elle a lancés. La sortie est écrite dans `logs\jobs\<site>.<name>.log`, affiché par la page Logs. Avec le service Windows, le compte du service exécute les tâches et l'intercepteur d'e-mails, comme il exécute PHP.

## Node.js

**Install Node.js** sur la page Node.js liste les versions Current et LTS de nodejs.org. Un téléchargement est vérifié avec `SHASUMS256.txt` et décompressé dans `node\<version>`. La page liste aussi les installations de Node.js trouvées dans `Program Files`, Laragon, nvm, Volta et dans le `PATH`. **Make default** choisit la version qui exécute les applications Node.js, et les applications sont listées en dessous avec **Restart** et **Log**.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../panel-node-dark.png">
  <img src="../panel-node-light.png" width="880" alt="Page Node.js du tableau de bord avec les versions de Node.js installées et les applications en cours d'exécution">
</picture>

Une application Node.js n'a besoin d'aucune modification : `app.listen(3000)` est redirigé vers l'adresse que lui attribue le serveur. Voir [docs/APPS.md](../APPS.md).

Chaque site peut utiliser son propre Node.js : choisissez-le dans la colonne Version de la page Sites, ou exécutez `openlitespeed --site node <site> <node.exe>`. Sans choix, un fichier `.nvmrc` ou `.node-version` qui indique une version majeure comme `20` sélectionne le Node.js 20 installé le plus récent ; sinon, le site s'exécute avec la version par défaut. Les tâches en arrière-plan et le terminal du site utilisent la même version.

## Migrer depuis Apache

OpenLiteSpeed lit une configuration Apache existante et crée un site pour chaque `<VirtualHost>`, avec ses domaines, sa racine des documents, son certificat SSL, ses redirections, ses règles de réécriture, ses alias, ses proxys, ses règles d'accès et sa protection par mot de passe. Les règles des fichiers `.htaccess` continuent de fonctionner, car OpenLiteSpeed les applique lui-même.

```bat
bin\openlitespeed.exe --import-apache --dry-run
bin\openlitespeed.exe --import-apache C:\Apache24\conf\httpd.conf
bin\openlitespeed.exe -s reload
```

Sans chemin, la configuration Apache de Laragon, XAMPP, WampServer ou `C:\Apache24` est trouvée automatiquement. `--dry-run` affiche le rapport sans rien modifier. Un nouvel import remplace le précédent, et `--import-apache --remove` le retire. Les règles qui ne peuvent pas être converties sont listées dans le rapport. Les règles d'accès sont converties à l'identique ou de façon plus stricte, jamais plus permissive ; la seule exception, les règles qui bloquent des clients précis par nom d'hôte ou variable d'environnement, est signalée par un avertissement. Les détails se trouvent dans [docs/APACHE.md](../APACHE.md).

## Migrer depuis nginx

`--import-nginx` fait de même pour nginx : chaque bloc `server` devient un site avec ses domaines, ses écouteurs, son certificat SSL, ses locations, `try_files`, ses réécritures, ses redirections, ses alias, ses proxys, ses règles d'accès et sa protection par mot de passe, et le PHP derrière `fastcgi_pass` s'exécute avec le PHP d'OpenLiteSpeed.

```bat
bin\openlitespeed.exe --import-nginx --dry-run
bin\openlitespeed.exe --import-nginx C:\nginx\conf\nginx.conf
```

Les détails se trouvent dans [docs/NGINX.md](../NGINX.md).

## Plusieurs cœurs de processeur

OpenLiteSpeed démarre un worker HTTP par cœur de processeur, jusqu'à 8 par défaut. Réglez **Number of Workers** sous **Server Configuration > General** dans WebAdmin, ou `httpdWorkers` dans `conf\httpd_config.conf`, sur une valeur de 1 à 32. Chaque worker a son propre pool PHP. Consultez [docs/ARCHITECTURE.md](../ARCHITECTURE.md) pour savoir comment les workers partagent les sockets d'écoute sous Windows.

## Ligne de commande

Exécutez les commandes dans le dossier d'installation sous la forme `bin\openlitespeed.exe`.

| Commande | Description |
|---|---|
| `openlitespeed -s start\|stop\|restart\|reload\|status` | Contrôler le serveur, `reload` est un redémarrage sans interruption de service |
| `openlitespeed -d` | Exécuter au premier plan |
| `openlitespeed --install-service` / `--uninstall-service` | Inscrire ou supprimer le service Windows |
| `openlitespeed --set-php <folder>` | Utiliser une autre version de PHP |
| `openlitespeed --import-apache [httpd.conf] [--dry-run]` / `--import-apache --remove` | Importer les sites Apache / supprimer les sites importés |
| `openlitespeed --import-nginx [nginx.conf] [--dry-run]` / `--import-nginx --remove` | Importer les sites nginx / supprimer les sites importés |
| `openlitespeed --acme-install [--email <address>]` | Activer les certificats Let's Encrypt automatiques |
| `openlitespeed --modsec-install [--detection-only]` / `--modsec-install --remove` | Activer ou désactiver le pare-feu ModSecurity avec l'OWASP Core Rule Set |
| `openlitespeed --dev-sites <folder> [tld]` | Créer un site pour chaque sous-dossier d'un dossier |
| `openlitespeed --new-site <name> [--wordpress\|--laravel\|--node\|--python]` | Créer un site dans le dossier des projets : un site PHP, WordPress avec sa base de données, un projet Laravel, une application Node.js ou une application Python |
| `openlitespeed --site php <site> <folder>\|default` | Utiliser une autre version de PHP pour un site |
| `openlitespeed --site app <site> node\|python\|off` | Exécuter le dossier d'un site comme application Node.js ou Python, ou de nouveau comme site PHP |
| `openlitespeed --site node <site> <node.exe>\|default` | Utiliser une autre version de Node.js pour un site |
| `openlitespeed --php list` | Les versions de PHP de cet ordinateur en JSON, avec les sites et l'état de Xdebug de chacune |
| `openlitespeed --php available` / `install <8.x> [ts]` / `remove <folder>` | Lister et installer les versions de PHP de windows.php.net, supprimer une version téléchargée |
| `openlitespeed --php ext <folder>` / `ext-set <folder> <name> on\|off` | Lister les extensions d'une version de PHP, en activer ou désactiver une |
| `openlitespeed --php ini <folder>` / `ini-set <folder> <key> <value>` | Lire ou modifier un réglage courant de `php.ini` |
| `openlitespeed --php xdebug <folder> on\|off\|status` | Activer ou désactiver Xdebug pour une version de PHP, ou indiquer s'il est actif |
| `openlitespeed --node list\|available` / `install <major>` / `default <node.exe>` / `remove <node.exe>` | Gérer les versions de Node.js des applications |
| `openlitespeed --composer install` | Télécharger Composer dans `tools\composer` |
| `openlitespeed --set-port http\|admin <port>` | Changer le port du site web ou de WebAdmin |
| `openlitespeed --proxy add <name> <port>` / `remove <name>` / `list` | Ajouter ou supprimer un site proxy pour une application sur un port local |
| `openlitespeed --dev-https on [port]\|off\|status` | HTTPS de confiance pour les sites locaux |
| `openlitespeed --dev-hosts` / `--dev-hosts-clear` | Ajouter ou supprimer les noms des sites dans le fichier hosts |
| `openlitespeed --db install\|fetch\|start\|stop\|status` | Installer, seulement télécharger, démarrer ou arrêter MariaDB et phpMyAdmin |
| `openlitespeed --db list` / `create <name> [--no-user]` / `drop <name> [--user]` | Lister, créer ou supprimer des bases de données |
| `openlitespeed --db export <name> <file.sql>` / `import <name> <file.sql>` | Exporter une base de données dans un fichier ou importer un fichier dans celle-ci |
| `openlitespeed --tunnel start <site>` / `stop <site>` / `stop --all` / `status` | Mettre un site en ligne avec Cloudflare Tunnel ou le retirer |
| `openlitespeed --tunnel token` / `run` / `run-stop` / `token clear` | Enregistrer un jeton de tunnel depuis l'entrée standard, connecter ou déconnecter votre propre domaine, oublier le jeton |
| `openlitespeed --mail on\|off\|status` | Activer ou désactiver l'intercepteur d'e-mails, ou indiquer s'il fonctionne |
| `openlitespeed --mail list` / `remove <id>` / `save <id> <n> <file>` / `clear` | Lister les e-mails interceptés en JSON, supprimer un message, enregistrer la pièce jointe `n` (à partir de 0) d'un message dans un fichier, ou supprimer tous les messages |
| `openlitespeed --mail sendmail [-t] [-i] [-f <from>] [<recipient>...]` | Enregistrer un message depuis l'entrée standard, comme sendmail |
| `openlitespeed --job list` / `add <site> <name> <command>` / `remove <site> <name>` | Lister, ajouter ou supprimer les tâches en arrière-plan des sites |
| `openlitespeed --job start\|stop\|restart <site> <name>` / `autostart <site> <name> on\|off` | Contrôler une tâche, choisir si elle démarre avec le serveur web |
| `openlitespeed --status` | Serveur, PHP, sites, base de données et tunnels en JSON |
| `openlitespeed --admin-password admin\conf\htpasswd <password>` | Définir le mot de passe WebAdmin |

L'application de la zone de notification démarre, arrête et recharge le serveur, ouvre les sites, WebAdmin et les journaux, crée des sites et des sites proxy, active et désactive le HTTPS de confiance, change de version de PHP et importe les sites Apache et nginx. Dans l'édition full stack, elle ouvre aussi le tableau de bord et contrôle MariaDB. Une fois par jour, elle vérifie sur GitHub si une nouvelle version est disponible ; **Update** la télécharge, vérifie la signature et la somme de contrôle et lance le nouvel installateur. `check=0` sous `[update]` dans `conf\olstray.ini` désactive cette vérification.

## Compiler depuis les sources

Voir [docs/BUILD.md](../BUILD.md). La compilation utilise Visual Studio 2022, CMake et Ninja, et récupère toutes les dépendances à des commits figés.

## Limitations

- Pas de changement d'utilisateur par hôte virtuel (suEXEC). PHP s'exécute sous l'utilisateur qui exécute le serveur, ou sous le compte du service.
- Pas de `SO_REUSEPORT`. Les workers partagent un socket d'écoute par adresse.
- cgroups, namespaces, bubblewrap et CageFS sont des fonctionnalités de Linux et ne sont pas disponibles.
- Les applications Ruby via le contexte App Server ne sont pas prises en charge.

## Contribuer

Les rapports de bugs et les pull requests sont les bienvenus, voir [CONTRIBUTING.md](../../CONTRIBUTING.md). Signalez les problèmes de sécurité en privé comme indiqué dans [SECURITY.md](../../SECURITY.md).

## Licence

GPL-3.0, voir [LICENSE](../../LICENSE). OpenLiteSpeed est développé par LiteSpeed Technologies et distribué sous licence GPL-3.0. `lsphp.exe` est compilé à partir des sources de PHP et relève de la [PHP License](../../win/lsphp/PHP-LICENSE.txt). Les composants tiers sont listés dans [THIRD-PARTY-NOTICES.txt](../../win/package/THIRD-PARTY-NOTICES.txt).

Ce projet n'est ni affilié à LiteSpeed Technologies ou au PHP Group, ni approuvé par eux. LiteSpeed et OpenLiteSpeed sont des marques de LiteSpeed Technologies, Inc.

Du même auteur : [OpenLiteSpeed for cPanel and WHM](https://github.com/NeuralFoundry/openlitespeed-cpanel).
