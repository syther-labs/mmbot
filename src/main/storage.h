/*
 * storage.h
 *
 *  Created on: 16. 5. 2019
 *      Author: ondra
 */

#ifndef SRC_MAIN_STORAGE_H_
#define SRC_MAIN_STORAGE_H_
#include <memory>

#include <imtjson/value.h>
#include "istorage.h"


class Storage: public IStorage {
public:

	enum Format {
		json,
		jsonp,
		binjson
	};


	Storage(std::string file, int versions, Format format);

	virtual void store(json::Value data) override;
	virtual json::Value load() override;


protected:
	std::string file;
	int versions;
	Format format;

};


class StorageFactory {
public:

	StorageFactory(std::string path):path(path),versions(5),format(Storage::json) {}
	StorageFactory(std::string path, bool binary):path(path),versions(5),format(binary?Storage::binjson:Storage::json) {}
	StorageFactory(std::string path, int versions, Storage::Format format):path(path),versions(versions),format(format) {}
	PStorage create(std::string name) const;


protected:
	std::string path;
	int versions;
	Storage::Format format;
};

#endif /* SRC_MAIN_STORAGE_H_ */