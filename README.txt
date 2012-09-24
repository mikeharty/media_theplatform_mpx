Media: thePlatform mpx
======================

* Creates a Media:mpx PHP Stream Wrapper for Media 

* Imports mpxPlayers and mpxMedia from a specified mpx account on thePlatform.com


-- REQUIREMENTS --

* An administrator account on thePlatform.com

* Media 7.x-1.x (http://drupal.org/project/media)

* Media Internet, File Entity (included with Media)


-- CONFIGURATION --

1. After installing Media: tpMpx, Enter your mpx account information at Administration > Configuration > Media > Media: thePlatform mpx Settings

2. Upon successful signin, select an Import Account to use for importing mpxPlayers and mpx Feeds.

3. Upon setting an Import Account, all mpxPlayers from the mpx Account on thePlatform will be imported into the Drupal Media Library. You may view them at Administration > Content > Media > mpxPlayers.

4. Go to Administration > Content > Media > mpxMedia. 

Configure the mpxMedia Settings:

* Select the Import Feed to use for mpxMedia. 
This list contains all of the mpx Feeds in your account which are enabled.

* Select the Default mpxPlayer to use for displaying mpxMedia.

5. Upon saving mpxMedia Settings, click "Sync mpxMedia Now" to import all mpx mpxMedia from the mpx Feed into your Drupal Media Library.

6. In Administration > Configuration > File Displays, be sure to check 'thePlatform video' and 'thePlatform preview image' for Video in "Manage File Display" under in order to render mpxPlayers and mpxMedia.