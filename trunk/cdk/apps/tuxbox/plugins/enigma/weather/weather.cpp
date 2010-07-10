/* 
Dreambox weather plugin
Copyright (C) 2004 Bjorn Hijmans (bjorn@hijmans.nl)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <weather.h>
#include <lib/gdi/epng.h>
#include <lib/socket/dpopen.h>


extern "C" int plugin_exec( PluginParam *par );

int plugin_exec( PluginParam *par )
{	weatherMain dlg;

	dlg.show();
	int result=dlg.exec();
	dlg.hide();

	return result;
}

const char * mystrcasestr(const char *haystack, const char *needle)
{
    int needleLen = strlen( needle );
    int matchLen = strlen( haystack ) - needleLen + 1;

    // If needle is bigger than haystack, can't match
    if ( matchLen < 0 )
        return NULL;

    if ( needleLen == 0 )
        return NULL;

    while ( matchLen ) {

        // See if the needle matches the start of the haystack
        int i;
        for ( i = 0; i < needleLen; i++) {
            if ( tolower( haystack[i] ) != tolower( needle[i] ) )
		  break;
        }

        // If it matched along the whole needle length, we found it
        if ( i == needleLen)
            return haystack;

        matchLen--;
        haystack++;
    }

    return NULL;
}


void weatherMain::selectLocation()
{	// Display location select window
	this->hide();

	theLocationSelect->setLBFocus();
	theLocationSelect->show();
	theLocationSelect->exec();

	if(theLocationSelect->selectedURL != "")
	{	theFetcher->url = theLocationSelect->selectedURL;
	}
	else
	{	theFetcher->url = theConfigParser->url;
	}

	if(theLocationSelect->selectedName != "")
	{	setText(theLocationSelect->selectedName);
	}

	theFetcher->fetch();
}

void weatherMain::dispData()
{
	dispDataPage(0);
}

void weatherMain::dispDataForward()
{
	dispDataPage(startpos+3);
}
void weatherMain::dispDataBack()
{
	dispDataPage(startpos-3);
}

void weatherMain::dispDataPage(int first)
{	eString desc, icon;
	int factor = 197;
	int itemcount=weatherItems.size();
	if(first>=0 && first<itemcount){
		clear();
		startpos=first;
	}else   return;
		
	setFocus(btn1);

	this->show();

	btn1->move(ePoint(10, 360));
	btn1->resize(eSize(150, 30));
	btn1->setShortcut("green");
	btn1->setShortcutPixmap("green");
	btn1->loadDeco();
	btn1->setText(_("Select location"));
	btn1->setHelpText(_("Click to select another location"));

	btnB->move(ePoint(250, 360));
	btnB->resize(eSize(150, 30));
	btnB->setShortcut("yellow");
	btnB->setShortcutPixmap("yellow");
	btnB->loadDeco();
	btnB->setText(_("Last Page"));

	btnF->move(ePoint(430, 360));
	btnF->resize(eSize(150, 30));
	btnF->setShortcut("blue");
	btnF->setShortcutPixmap("blue");
	btnF->loadDeco();
	btnF->setText(_("Next Page"));
	
	if(!first)
		btnB->hide();
	else
		btnB->show();
	if(first+3>(itemcount-1))
		btnF->hide();
	else
		btnF->show();

	std::list<WeatherItem>::iterator i;
	int j = 0,k=0;
	for(i = weatherItems.begin(); i != weatherItems.end() && j < 3; ++i,++k)
	{
		if(k<startpos)continue;
		theConfigParser->LookUp(i->description, desc, icon);
		lb1->move(ePoint((j * factor), 20));
		lb1->resize(eSize(factor, 40));
		lb1->setProperty("align", "center");
		lb1->setText(i->date);

		lb2->move(ePoint((j * factor), 270));
		lb2->resize(eSize(factor, 80));
		lb2->setProperty("align", "center");
		lb2->setProperty("wrap", "on");
		lb2->setText(desc);
		if(!access(("/var/tuxbox/config/weather/icons/" + icon).c_str(),0))
			icon = "/var/tuxbox/config/weather/icons/" + icon;
		else
			icon = "/share/tuxbox/weather/" + icon;
		gPixmap *img = loadPNG(icon.c_str());


		if(img)
		{	gPixmap * mp = &gFBDC::getInstance()->getPixmap();
			gPixmapDC mydc(img);
			gPainter p(mydc);
			p.mergePalette(*mp);

			lb3->move(ePoint((j * factor) + 30, 90));
			lb3->resize(eSize(factor-40, 400));
			lb3->setBlitFlags(BF_ALPHATEST);
			lb3->setProperty("align", "center");
			lb3->setPixmap(img);
			lb3->setPixmapPosition(ePoint(1, 1));
		}
		else
		{	eMessageBox msg(_("Error loading weather icon"), _("Error"), eMessageBox::iconWarning|eMessageBox::btOK);
			msg.show();     msg.exec();     msg.hide();
		}

		lb5->move(ePoint(65 + (j * factor), 310));
		lb5->resize(eSize(30, 40));
		lb5->setText(eString().sprintf("%d", i->min_temp));

		lb6->move(ePoint(95 + (j * factor), 310));
		lb6->resize(eSize(30, 40));
		lb6->setText("~");

		lb7->move(ePoint(125 + (j * factor), 310));
		lb7->resize(eSize(30, 40));
		lb7->setText(eString().sprintf("%d", i->max_temp));

		j++;
	}
	if(j == 0)
	{	eMessageBox msg1(_("No items to display"), _("Timeout"), eMessageBox::iconWarning|eMessageBox::btOK);
		msg1.show();     msg1.exec();     msg1.hide();
	}
}

weatherMain::weatherMain(): eWindow(1),startpos(0)
{	cmove(ePoint(70, 50));
	cresize(eSize(590, 400));
	

	theConfigParser = new ConfigParser;

	setText(theConfigParser->name);	

	theLocationSelect = new locationSelect(theConfigParser);

	if(theLocationSelect->selectedURL != "")
	{	theFetcher = new Fetcher(theLocationSelect->selectedURL);
	}
	else
	{		theFetcher = new Fetcher(theConfigParser->url);
	}

	if(theLocationSelect->selectedName != "")
	{	setText(theLocationSelect->selectedName);
	}

	CONNECT(theFetcher->downloadDone, weatherMain::downloadDone);
	CONNECT(theFetcher->downloadTimeout, weatherMain::downloadTimeout);
	theFetcher->fetch();

	lb1 	= new eLabel(this);
	lb2 	= new eLabel(this);
	lb3 	= new eLabel(this);
	lb4 	= new eLabel(this);
	lb5 	= new eLabel(this);
	lb6 	= new eLabel(this);
	lb7	= new eLabel(this);

	btn1 = new eButton(this);
	btnB= new eButton(this);
	btnF= new eButton(this);
	CONNECT(btn1->selected, weatherMain::selectLocation);	
	CONNECT(btnB->selected, weatherMain::dispDataBack);	
	CONNECT(btnF->selected, weatherMain::dispDataForward);	
	this->hide();
}

weatherMain::~weatherMain()
{
}

void weatherMain::downloadDone(int err)
{	theLocationSelect->hide();
	parse("/var/tmp/weather.tmp");
	dispData();
}

void weatherMain::downloadTimeout(int err)
{	theLocationSelect->hide();

	eMessageBox msg1(_("Timeout downloading weather data"), _("Timeout"), eMessageBox::iconWarning|eMessageBox::btOK);
	msg1.show();     msg1.exec();     msg1.hide();		

	dispData();
}

void weatherMain::parse(eString file)
{	XMLTreeParser * parser;

	if(!access("/var/tuxbox/config/weather/format_weather.sh",X_OK)){
		char cmdstr[1024];
		sprintf(cmdstr,"/var/tuxbox/config/weather/format_weather.sh %s",file.c_str());
		system((const char *)cmdstr);
	}

	FILE *in = fopen(file.c_str(), "rt");

	weatherItems.clear();

	if(in) 
	{	parser = new XMLTreeParser("UTF-8");
		char buf[2048];

		int done;
		do 
		{	unsigned int len=fread(buf, 1, sizeof(buf), in);
			done = ( len < sizeof(buf) );
			if ( ! parser->Parse( buf, len, done ) ) 
			{	eMessageBox msg(_("Weatherfile parse error"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		                msg.show();     msg.exec();     msg.hide();
				delete parser;
				parser = NULL;
				return;
			}
		} 
		while (!done);

                fclose(in);

		XMLTreeNode * root = parser->RootNode();
		if(!root)
		{	eMessageBox msg(_("Weatherfile parse error"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		        msg.show();     msg.exec();     msg.hide();	
			return;
		}
		if ( strcmp( root->GetType(), "rdf:RDF") ) 
		{	eMessageBox msg(_("Invalid weatherfile"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		        msg.show();     msg.exec();     msg.hide();
	                return;
	        }
	
		for(XMLTreeNode * node = root->GetChild(); node; node = node->GetNext())
		{	if(!strcmp(node->GetType(), "item"))
			{	WeatherItem thisItem;
				for(XMLTreeNode * i = node->GetChild(); i; i = i->GetNext())
		   		{	if(!strcmp(i->GetType(), "title"))
   					{	if(strcmp(i->GetData(), "Colophon"))
						{	thisItem.date = i->GetData();
						}
		   			}
   					if(!strcmp(i->GetType(), "description"))
   					{	if(thisItem.date != "")
						{	getDescription(i->GetData(), &thisItem.description);
	   						thisItem.min_temp = getTemp(i->GetData(), "min");
			   				thisItem.max_temp = getTemp(i->GetData(), "max");
						}
   					}
   				}
				if(thisItem.date != "")
				{	weatherItems.push_back(thisItem);
				}
			}
		}


		delete parser;
	}
	else
	{	eMessageBox msg(_("Weatherfile not found"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		msg.show();     msg.exec();     msg.hide();
	}
}

int weatherMain::getTemp(eString description, eString key)
{	int tmp = 0;
	for(unsigned int i = 0; i < description.length() - 4; i++)
	{	if(description.mid(i, 3) == key.c_str())
		{	int start = i + 4;
			for(unsigned int j = i + 4; i < description.length() - 4; j++)
			{	if(description.mid(j, 3) == "deg")
				{	int stop = j - 1;
					tmp = atoi(description.mid(start, stop - start).c_str());
					break;
				}
			}
			break;
		}
	}
	return tmp; 
}

void weatherMain::getDescription(eString description, eString *out)
{
	for(unsigned int i = 0; i < description.length()-4; i++)
	{	if(description.mid(i, 4) == "<br>")
		{	*out = description.mid(0, i);
			return;
		}
	}
	for(unsigned int i = 0; i < description.length(); i++)
	{	if(description.mid(i, 1) == ",")
		{	*out = description.mid(0, i);
			return;
		}
	}
}

locationSelect::locationSelect(ConfigParser * t) : eWindow(1), theConfigParser(t)
{	cmove(ePoint(140, 140));
	cresize(eSize(440, 296));
	setText("Select location");

	theList = new eListBox<eListBoxEntryText>(this);
	theList->move(ePoint(10, 10));
	theList->resize(eSize(clientrect.width() - 20, clientrect.height() - 20));
	theList->loadDeco();
	theList->setColumns(1);

	CONNECT(theList->selected, locationSelect::selectedItem);

	int selected=false;
	FILE *f=fopen("/var/tuxbox/config/weather/weather.cfg","rt");
	if(f){
		unsigned char Name[1024];
		if(1==fscanf(f,"selectedName=%s",Name)){
			selectedName=(const char *)Name;
			selected=true;
		}
		fclose(f);
	}

	std::list<LocationItem>::iterator i;
	for(i = theConfigParser->locationItems.begin(); i != theConfigParser->locationItems.end(); ++i)
	{	new eListBoxEntryText(theList, i->name.c_str(), (void *) &(i->id));
		if(i->name==selectedName && selected)
			selectedURL=i->url;
	}


}

void locationSelect::setLBFocus()
{	setFocus(theList);
}


void locationSelect::selectedItem(eListBoxEntryText *item)
{	if(item)
	{	int * iKey = (int *) (item->getKey());
		std::list<LocationItem>::iterator i;
		for(i = theConfigParser->locationItems.begin(); i != theConfigParser->locationItems.end(); ++i)
		{	if(i->id == *iKey)
			{	selectedURL = i->url;
				selectedName = i->name;
				FILE *f=fopen("/var/tuxbox/config/weather/weather.cfg","wt");
				if(f){
					fprintf(f,"selectedName=%s",selectedName.c_str());
					fclose(f);
				}
			}
		}
		reject();
	}
}

Fetcher::Fetcher(eString url) : 
	url(url), timeout(eApp)
{
}

void Fetcher::fetch()
{	timerExpired = 0;
	tempPath = "/var/tmp/weather.tmp";
	int error = 0;
	connectionP = eHTTPConnection::doRequest(url.c_str(), eApp, &error);
	
	if(!connectionP || error)
	{	eMessageBox msg("Error downloading " + url + "(" + eString().sprintf("%d", error) + ")", _("Details"), eMessageBox::btOK);
		msg.show();     msg.exec();     msg.hide();
	}
	else
	{	timeout.start(15000, true); // connection timeout
		CONNECT(timeout.timeout, Fetcher::connectionTimeouted);
	                
		CONNECT(connectionP->transferDone, Fetcher::transferDone);
		CONNECT(connectionP->createDataSource, Fetcher::createDownloadSink);
		connectionP->local_header["User-Agent"] = "RSS";
		connectionP->start();
	}
}

void Fetcher::connectionTimeouted()
{	timeout.stop();
	if(connectionP)
	{	delete connectionP;
		connectionP = NULL;
		timerExpired = 1;
		downloadTimeout(0);
	}		
}

void Fetcher::transferDone(int err)
{	timeout.stop();
	if(!timerExpired)
	{	if(!err)
		{	connectionP = NULL;
			// Tell caller download is ready
			/*emit*/ downloadDone(err);
		}
		else
		{	eString sMsg = "Error " + eString().sprintf("%d", err);
			eMessageBox msg(sMsg, _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
			msg.show();     msg.exec();     msg.hide();
		}
	}
}

eHTTPDataSource * Fetcher::createDownloadSink(eHTTPConnection *conn)
{	dataSinkP = new eHTTPDownload(connectionP, (char *)tempPath.c_str());

	return(dataSinkP);
}

void ConfigParser::LookUp(eString orig, eString &desc, eString &icon)
{	desc = "";
	icon = "";

	std::list<ConfigItem>::iterator i;

	for(i = configItems.begin(); i != configItems.end(); ++i)
	{	if(i->origdescription.upper() == orig.upper())
		{       desc = i->description;
			icon = i->icon;
		}
	}
	if(icon == "")
	{	// Nothing found, try a less exact match
		for(i = configItems.begin(); i != configItems.end(); ++i)
		{	eString sMsg = "Looking for " + orig + " and found " + i->origdescription + " and " + i->icon;
			if(mystrcasestr(orig.upper().c_str(), i->origdescription.upper().c_str()))
			{       desc = i->description;
				icon = i->icon;
			}
		}
	}
	if(icon == "")
	{	icon = "unknown.png";
	}
	if(desc == "")
	{	
		unsigned int pos = orig.find("<br>",0);
		if(pos!=std::string::npos && pos)
			desc=orig.left(pos);
		else		
			desc = orig;
	}
}

ConfigParser::ConfigParser()
{	XMLTreeParser * parser;

	FILE *in = fopen("/var/tuxbox/config/weather/weather.xml", "rt");
	if (!in)
		in = fopen(CONFIGDIR"/weather.xml", "rt");
	
	if(in)
	{	parser = new XMLTreeParser("UTF-8");
		char buf[2048];

		int done;
		do 
		{	unsigned int len=fread(buf, 1, sizeof(buf), in);
			done = ( len < sizeof(buf) );
			if ( ! parser->Parse( buf, len, done ) ) 
			{	eMessageBox msg(_("Configfile parse error"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		                msg.show();     msg.exec();     msg.hide();
				delete parser;
				parser = NULL;
				return;
			}
		} 
		while (!done);

                fclose(in);

		XMLTreeNode * root = parser->RootNode();
		if(!root)
		{	eMessageBox msg(_("Configfile parse error"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		        msg.show();     msg.exec();     msg.hide();	
			return;
		}
		if ( strcmp( root->GetType(), "config") ) 
		{	eMessageBox msg(_("Invalid configfile"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		        msg.show();     msg.exec();     msg.hide();
	                return;
	        }

		ConfigItem thisItem;
		LocationItem thisLocation;
		int idcnt = 0;

		for(XMLTreeNode * node = root->GetChild(); node; node = node->GetNext())
		{	if(!strcmp(node->GetType(), "mapping"))
			{	for(XMLTreeNode * i = node->GetChild(); i; i = i->GetNext())
				{	if(!strcmp(i->GetType(), "origdescription"))
					{	thisItem.origdescription = i->GetData();
					}
					if(!strcmp(i->GetType(), "description"))
					{	thisItem.description = i->GetData();
					}
					if(!strcmp(i->GetType(), "icon"))
					{	thisItem.icon = i->GetData();
					}
				}
				configItems.push_back(thisItem);
			}
			if(!strcmp(node->GetType(), "locations"))
			{	for(XMLTreeNode * item = node->GetChild(); item; item = item->GetNext())
				{	if(!strcmp(item->GetType(), "location"))
					{	for(XMLTreeNode * i = item->GetChild(); i; i = i->GetNext())
						{	if(!strcmp(i->GetType(), "name"))
							{	thisLocation.name = i->GetData();
							}
							if(!strcmp(i->GetType(), "url"))
							{	thisLocation.url = i->GetData();
							}
			
						}
						thisLocation.id = idcnt++;
						if(url == "")
						{	url = thisLocation.url;
							name = thisLocation.name;
						}
						locationItems.push_back(thisLocation);	
					}
				}
			}
		}

		delete parser;
	}
	else
	{	eMessageBox msg(_("Config file not found"), _("User Abort"), eMessageBox::iconWarning|eMessageBox::btOK);
		msg.show();     msg.exec();     msg.hide();
	}
}
