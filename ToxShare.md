#Tox Sharing Protocol

Current limitation

* Only single file.
* Sender is always the one who start the exchange. As an example, avatar is send many time to same client, that often verify by comparing the hash that's a different one.
* Both sender and receiver have to save state of their file queue in case of connection lost or client close. No standard to save state is provided yet.
* no method to request file, or a list of, to give client software the capacity of show files shared and request it without need sender action.
* since the crypto API ensures that no packet are lost or corrupted, the only way tox client verify that a file is the same is by filesize. It's not robust since the file can have changed and keep the same size (as logrotate one). We could also want to resume a file from another friend/toxid. Client have to allow resume, check if the file is same asking a chunk it already have, and compare the chunk checksum with a request one.

https://github.com/irungentoo/Tox_Client_Guidelines/blob/master/Important/File_Transfers.md
https://wiki.theory.org/BitTorrentSpecification#Bencoding

State of client

* https://github.com/tux3/qTox/issues/1532
* claim to be supported by uTox

Consideration on file sharing

* To allow sharing different files with different friends, you've to create group-like right on directory you want to share. A tag system is good to do that : #work, #sport, #tox...
* Monitoring filesystem events and rebuild the file list in consequence.
* A standard protocol for file sharing is necessary 

