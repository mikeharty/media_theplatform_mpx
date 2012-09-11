Media: thePlatform mpx
======================

* Creates a Media:mpx PHP Stream Wrapper for Media 

* Imports mpx Players and Videos from a specified mpx account on thePlatform.com


-- REQUIREMENTS --

* An administrator account on thePlatform.com

* Media 7.x-1.x (http://drupal.org/project/media)

* Media Internet, File Entity (included with Media)


-- CONFIGURATION --

1. After installing Media: tpMpx module, enter your mpx account information at Administration > Configuration > Media > Media: thePlatform mpx Settings

2. Upon successful signin, select an Import Account to use for importing mpx Players and mpx Feeds.

3. Upon setting an Import Account, all mpx Players from the mpx Account on thePlatform will be imported into the Drupal Media Library.

4. Go to Administration > Content > Media > mpx Players. Configure the Player Settings:
 * Select the Default Player to use for mpx Videos.
 * Select the Runtimes to use for Players.
 * Check whether or not to Sync Players on cron.
 
5. Go to Administration > Content > Media > mpx Videos. Configure the Video Settings:

 * Select the Import Feed to use for mpx Videos. This list contains all of the mpx Feeds in your account which are enabled.
 * Check whether or not Sync Videos on cron.
 
6. Upon setting an Import Feed in the Video Settings, click "Sync Videos Now" to import all mpx Videos from the mpx Feed into your Media Library.

7. Be sure to check 'thePlatform video' and 'thePlatform preview image' for Video in "Manage File Display" under Administration > Configuration > File Displays in order to render mpx File Entities.