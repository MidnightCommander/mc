
== Pre-requirements ==

=== List of required apps ===

* tx (A transifex client. http://help.transifex.com/features/client/#user-client-08-install)
* po4a (A tool maintaining translations anywhere. http://alioth.debian.org/projects/po4a/)


=== An artifacts configuration ===

Put in the ~/.transifexrc file these lines:

[https://www.transifex.net]
hostname = https://www.transifex.net
username = YourTxLogin
password = YourTxPassword
token = 

== Interact with Transifex via scripts ==

To get all translations from Transifex run:

find ./ -name '*-fromTransifex.*' -exec {} \;

To put source files to Transifex run:

find ./ -name '*-toTransifex.*' -exec {} \;

