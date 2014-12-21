#!/usr/bin/env python
# 
# Copyright (c) 2014 Poul-Henning Kamp
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# 
# Postprocess simulation output for gnuplot
# =========================================
#
#	python plotgen.py
# then
#	gnuplot
#	load '/tmp/_g'
#

from __future__ import print_function

fi = open("/tmp/_")
fg = open("/tmp/_g", "w")

fg.write("#\n")

plots = []

class plot(object):
	def __init__(self, title = "-"):
		self.s = []
		self.title = title
		plots.append(self)

	def write(self, l):
		self.s.append(l)

	def commit(self, fg, title):
		if title:
			fg.write("set y2label '%s'\n" % self.title)
		else:
			fg.write("unset y2label\n")

		for i in self.s:
			fg.write(i + "\n")

now = 0.0

class simpll(object):
	def __init__(self):
		self.fo = open("/tmp/_simpll", "w")
		self.pl1 = plot('p/freq')
		self.pl1.write(
		    "plot '/tmp/_simpll' using 1:($2*1e6) with line notitle")
		self.pl2 = plot('p/off')
		self.pl2.write(
		    "plot '/tmp/_simpll' using 1:($3*1e6/($4 == 0 ? 1 : $4)) with impulse notitle")
		if False:
			self.pl3 = plot('p/dur')
			self.pl3.write(
			    "plot '/tmp/_simpll' using 1:4 with imp notitle")

	def data(self, ll):
		self.fo.write("%f " % now + " " + " ".join(ll[1:]) + "\n")

class combine(object):
	def __init__(self):
		self.fo = open("/tmp/_combine", "w")
		self.pl1 = plot("c/in")
		self.pl1.write(
		    "plot '/tmp/_combine' using 1:2 notitle"
		    ", '/tmp/_combine' using 1:4 notitle"
		    ", '/tmp/_combine' using 1:3 notitle"
		)
		self.pl2 = plot('c/peak')
		self.pl2.write(
		    "plot '/tmp/_combine' using 1:5 with line notitle"
		)
		self.pl3 = plot("c/weight")
		self.pl3.write("set yrange[0:]")
		self.pl3.write(
		    "plot '/tmp/_combine' using 1:6 axis x1y2 with line notitle"
		)
		self.pl3.write("set autoscale y")

	def data(self, ll):
		self.fo.write("%f " % now + " " + " ".join(ll[3:]) + "\n")

p_simpll = simpll()

p_combine = combine()

n = 0
for l in fi:
	n += 1
	if len(l) == 0 or l[0] == "#":
		continue
	ll = l.split()
	if ll[0] == "Now":
		now = float(ll[1]) - 1e6
		continue
	if ll[0] == "SIMPLL":
		p_simpll.data(ll)
		continue
	if ll[0] == "Combine":
		p_combine.data(ll)
		continue

def plotset(xlo, xhi, lbl):
	fg.write("set autoscale x\n")
	fg.write("set xrange [%g:%g]\n" % (xlo, xhi))
	fg.write("set bmargin 0\n")
	fg.write("set xtics format ''\n")
	fg.write("set xtics (%g, %g)\n" % (xlo, xhi))
	for i in plots[:-1]:
		i.commit(fg, lbl)
	fg.write("set bmargin 2\n")
	fg.write("set xtics format '%g'\n")
	fg.write("set xtics (%g, %g)\n" % (xlo, xhi))
	plots[-1].commit(fg, lbl)

fg.write("set grid\n")
fg.write("set pointsize .5\n")
fg.write("set size .8,.8\n")
fg.write("set tmargin 1\n")
fg.write("set lmargin 8\n")

fg.write("set multiplot layout %d,3 columnsfirst\n" % len(plots))

fg.write("set style data point\n")
plotset(0, 64, False)

if now > 10000:
	fg.write("set pointsize .2\n")
	plotset(60, 3600, False)
	fg.write("set pointsize .1\n")
	plotset(3600, now, True)
else:
	fg.write("set pointsize .5\n")
	plotset(60, 600, False)
	fg.write("set pointsize .2\n")
	plotset(5, now, True)

fg.write("unset multiplot\n")
fg.write("set autoscale\n")
fg.write("set xtics auto\n")
fg.write("set ytics auto\n")
