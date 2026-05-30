#!/bin/bash
gdk-pixbuf-csource --rle --name=$1_icon $1.png > $1_icon.h
