#!/bin/sh

curl -s http://radioparadise.com/xml/now.xml | grep '<coverart>' | sed -e 's/<coverart>//' -e 's!</coverart>!!'
 
