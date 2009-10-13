#include <lib/dvb/eventdata.h>

#define HILO(x) (x##_hi << 8 | x##_lo)

int eventData::CacheSize=0;
__u8 eventData::data[4108];

eventData::eventData(const eit_event_struct* e, int size, int type,int source)
	:EITdata(NULL),ByteSize(size&0xFF), type(type&0xFF),saved(0)
{
	this->source=source;
	if (!e)
		return;

	if (!size)
	{
		size = HILO(e->descriptors_loop_length) + EIT_LOOP_SIZE;
		ByteSize=size&0xFF;
	}
	EITdata = new __u8[size];
	CacheSize+=size;
	memcpy(EITdata, (__u8*) e, size);
}

const eit_event_struct* eventData::get() const
{
	if (source!=srGEMINI_EPGDAT)
		return (const eit_event_struct*)EITdata;

	int pos = EIT_LOOP_SIZE;
	int tmp = ByteSize-10;
	memcpy(data, EITdata, 10);
	int descriptors_length=0;
	__u32 *p = (__u32*)(EITdata+10);
	while(tmp>3)
	{
		descriptorMap::iterator it =
			descriptors.find(*p++);
		if ( it != descriptors.end() )
		{
			int b = it->second.second[1]+2;
			memcpy(data+pos, it->second.second, b );
			pos += b;
			descriptors_length += b;
		}
		tmp-=4;
	}
	ASSERT(pos <= 4108);
	((eit_event_struct *)data)->descriptors_loop_length_hi = (descriptors_length >> 8) & 0x0F;
	((eit_event_struct *)data)->descriptors_loop_length_lo = descriptors_length & 0xFF;
	return (eit_event_struct*)data;
	
}

int eventData::getSize()
{
	if (!EITdata) return 0;
	return HILO(((const eit_event_struct*)EITdata)->descriptors_loop_length) + EIT_LOOP_SIZE;
}

eventData::~eventData()
{
	if ( ByteSize && source==srGEMINI_EPGDAT)
	{
		CacheSize -= ByteSize;
		__u32 *d = (__u32*)(EITdata+10);
		ByteSize -= 10;
		while(ByteSize>3)
		{
			descriptorMap::iterator it =
				descriptors.find(*d++);
			if ( it != descriptors.end() )
			{
				descriptorPair &p = it->second;
				if (!--p.first) // no more used descriptor
				{
					CacheSize -= it->second.second[1];
					delete [] it->second.second;  	// free descriptor memory
					descriptors.erase(it);	// remove entry from descriptor map
				}
			}
			ByteSize -= 4;
		}
	}
	delete [] EITdata;
}

void eventData::load(FILE *f,int source)
{
	if (source!=srGEMINI_EPGDAT)
		return;

	int size=0;
	int id=0;
	__u8 header[2];
	descriptorPair p;
	fread(&size, sizeof(int), 1, f);
	while(size)
	{
		fread(&id, sizeof(__u32), 1, f);
		fread(&p.first, sizeof(__u16), 1, f);

		__u16 reserverd;
		fread(&reserverd,sizeof(__u16),1,f);

		fread(header, 2, 1, f);
		int bytes = header[1]+2;
		p.second = new __u8[bytes];
		p.second[0] = header[0];
		p.second[1] = header[1];
		fread(p.second+2, bytes-2, 1, f);
		descriptors[id]=p;
		--size;
		CacheSize+=bytes;
	}
	
}

void eventData::save(FILE *f)
{
}

serviceEpgInfo::serviceEpgInfo() : localMap(0)
{
}

serviceEpgInfo::~serviceEpgInfo()
{
	if (localMap)
	{
		for ( serviceMap::iterator it( localMap->begin() ); it != localMap->end(); it++ )
			delete it->second;

		localMap->clear();
		delete localMap;
	}
}

bool serviceEpgInfo::insert( uniqueEPGKey& key, eventData* data )
{
	bool ret = true;
	
	if (!localMap)
		localMap = new serviceMap;
	
	std::pair<serviceMap::iterator,bool> insertResult = localMap->insert(std::pair<uniqueEPGKey, eventData*>( key, data) );
	if ( !insertResult.second )
	{
		delete data;  // Insert in map failed, delete allocated memory
		ret = false;
	}
	
	return ret;
}

const eventData* serviceEpgInfo::getInfo( uniqueEPGKey& key )
{
	const eventData* ret = 0;

	if ( localMap )
	{
		serviceMap::iterator it( localMap->find(key) );
		if ( it != localMap->end() )
			ret = it->second;
	}
	return ret;
}
