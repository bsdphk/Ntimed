Ntimed -- Network Time Synchronization
======================================

What is this ?
~~~~~~~~~~~~~~

This is a preview/early-acces/alpha/buzzword-of-the-times release
of a new FOSS project written to gradually take over the world of
networked timekeeping.

The first step is a NTP protocol client daemon, 'Ntimed-client',
which will synchronize a systems clock to some set of NTP servers

If this catches on, support for slave servers, refclocks and other
protocols, such as PTP, can be added, subject to interest, skill,
time and money.

The overall architectural goals are the same as every other FOSS
project claims to follow:  Simplicity, Quality, Security etc. etc.
but I tend to think that we stick a little bit more closely to them.

This work is sponsored by Linux Foundation, partly in response to
the HeartBleed fiasco, and after studying the 300,000+ lines of
source-code in NTPD.  I concluded that while it *could* be salvaged,
it would be more economical, much faster and far more efficient to
start from scratch.

Ntimed is the result.


What should you do with this
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can take this code, compile it, run it, and it will steer your
computer's clock, but I am not going to encourage you to do that in
production yet -- unless you know what you are doing and why you
are doing it:  This is only a preview release.

Soon-ish, there will be full production-ready releases and
packages for your favourite operating system, but we are not
there yet.

But if you are willing to read C-source code to figure out what the
printouts mean or if you care about quality time-keeping or quality
programming, I would love to hear your feedback, reviews, and ideas.


Where can I read more ?
~~~~~~~~~~~~~~~~~~~~~~~

I maintain a blog-of-sorts about this project here:

	http://phk.freebsd.dk/time

There you will find information about theory, practice,
and the thinking that tries to bridge the gap between them.

Updates typically happen during weekends -- that is when I work on
Ntimed.


Who do I yell at and how ?
~~~~~~~~~~~~~~~~~~~~~~~~~~

Me, Poul-Henning Kamp.  Please send email to phk@Ntimed.org.


What happens next ?
~~~~~~~~~~~~~~~~~~~

The plan is to have the first production-ready release in Q1/2015.

Hopefully OS releases will then adopt Ntimed - first as an alternative
to, and later as replacement for NTPD in client applications.

It is not my intent to start and manage an entirely new FOSS project
around Ntimed.  Harlan from The Network Time Foundation has agreed
to adopt Ntimed and it will run in/with/parallel to the NTPD project.
Or something.  We still need to flesh out all those details.


How to compile
~~~~~~~~~~~~~~

Pull the source code over to the machine you want to play on, and::

	sh configure
	make

(That was fast, wasn't it ?  -- Amazing how slow things aren't
when you don't go hunting for 27 different FORTRAN compilers for
your C code.)


How to test
~~~~~~~~~~~

Stop ntpd if it is running, and then::

	./ntimed-client -t /tmp/somefile some_ntp_server some_other_ntp_server 

That should synchronize your clock to those servers.

Because this is a preview release, the process will not "daemonize"
into the background.

The '-t /tmp/somefile' arguments tells it to write a full blow-by-blow
tracefile, for analysis and debugging.

If something goes wrong, I'm going to ask you for a copy the tracefile.


What happens when you run it ?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After a few seconds, your clock will be stepped if necessary.
(In the final version, this is where the process daemonizes into the
background -- from which point you can trust your clock to be good.)

In the next 30-60 seconds, the PLL will eliminate any residual phase
error and from this point in time, your computer's clock should be
good to a few milliseconds - depending on the quality of the servers.

After about 5-10 minutes, the PLL will have integrated the
frequency error of your computer's crystal, and the PLL will
start to "stiffen" to minimize the amount of steering necessary
to keep the clock aligned to the servers.

If you are using distant or very distant servers, it will take longer
time before the PLL stiffens.


Packet traces and simulations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can also run the program two other ways::

	./ntimed-client --poll-server some_ntp_server some_other_ntp_server

This will *not* steer your clock, but it will query the servers as
if it should have steered your clock.  (Ntpd should *not* be running
at the same time.)  By default it terminates after 1800 seconds,
but you can control that with "-d 3600" for one hour etc.

If you save the output into a file (redirect stdout or use '-t filename'),
you can use it as input for a simulation run::

	./ntimed-client --sim-client -s filename -t /tmp/_

And then run::

	python plotgen.py

Then you can finally get some nice pictures to look at by::

	gnuplot
	load '/tmp/_g'


Tweaking parameters
~~~~~~~~~~~~~~~~~~~

Parameters can be examined and tweaked with '-p' arguments::

	-p '?'

Gives a list of available parameters, and you can get information about
each parameter::

	-p parameter_name

To set the parameter to a non-default value::

	-p parameter_name=new_value

Not everything which should be a parameter is yet, and there are
some unused dummy parameters there, just to make sure the macro-magic
works.


Thanks and acknowledegments
~~~~~~~~~~~~~~~~~~~~~~~~~~~

First and foremost a big thanks to Professor Dave L. Mills.

Thanks for being the first time-nut on the InterNETs, as we called
them back then.

Thanks for being an all-round pleasant fellow to work with.

Thanks for adopting my 'nanokernel' and 'refclock_oncore'.

But in particular thanks for lending me the most cantankerous LORAN-C
receiver the world have ever seen, at a time in my life where I
badly needed that a distraction to keep me sane.

A big thanks to the Linux Foundation for realizing that NTPD was
in dire straits after Dave Mills retired.

Thanks for giving me money and free hands to do what I thought was
best -- even though I am a "BSD-guy".

Thanks to Harlan Stenn for keeping the NTPD flame burning, however
stormy the last decade has been.

I trust The Network Time Foundation will take as good care of Ntimed
in the future, as it has taken care of NTPD in the past.

A special wave of the hat to John R. Vig for his famous Quartz
Crystal Tutorial.

And finally, a shout-out and thanks to time-nuts@ in general and
Tom Van Baak in particular, for being jolly and interesting company
for people who happen to care about nanoseconds, leap seconds,
choke-ring antennas and the finer points of SC- vs. AT-cut quartz
crystals.

*phk*
