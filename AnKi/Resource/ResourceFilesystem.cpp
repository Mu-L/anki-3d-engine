// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Resource/ResourceFilesystem.h>
#include <AnKi/Util/Filesystem.h>
#include <AnKi/Util/Tracer.h>
#include <ZLib/contrib/minizip/unzip.h>
#if ANKI_OS_ANDROID
#	include <android_native_app_glue.h>
#endif

namespace anki {

static Error tokenizePath(CString path, ResourceString& actualPath, ResourceStringList& includedWords, ResourceStringList& excludedWords)
{
	ResourceStringList tokens;
	tokens.splitString(path, '|');

	const PtrSize count = tokens.getSize();
	if(count != 1 && count != 2)
	{
		ANKI_RESOURCE_LOGE("Tokenization of path failed: %s", path.cstr());
		return Error::kUserData;
	}

	actualPath = tokens.getFront();

	// Further tokenization
	if(count == 2)
	{
		const ResourceString excludeInclude = *(tokens.getBegin() + 1);

		ResourceStringList tokens;
		tokens.splitString(excludeInclude, ',');

		for(const auto& word : tokens)
		{
			if(word[0] == '!')
			{
				ResourceString w(&word[1], word.getEnd());
				excludedWords.emplaceBack(std::move(w));
			}
			else
			{
				includedWords.emplaceBack(word);
			}
		}
	}

	return Error::kNone;
}

/// C resource file
class CResourceFile final : public ResourceFile
{
public:
	File m_file;

	Error read(void* buff, PtrSize size) override
	{
		ANKI_TRACE_SCOPED_EVENT(RsrcFileRead);
		return m_file.read(buff, size);
	}

	Error readAllText(ResourceString& out) override
	{
		ANKI_TRACE_SCOPED_EVENT(RsrcFileRead);
		return m_file.readAllText(out);
	}

	Error readU32(U32& u) override
	{
		ANKI_TRACE_SCOPED_EVENT(RsrcFileRead);
		return m_file.readU32(u);
	}

	Error readF32(F32& f) override
	{
		ANKI_TRACE_SCOPED_EVENT(RsrcFileRead);
		return m_file.readF32(f);
	}

	Error seek(PtrSize offset, FileSeekOrigin origin) override
	{
		return m_file.seek(offset, origin);
	}

	PtrSize getSize() const override
	{
		return m_file.getSize();
	}
};

/// ZIP file
class ZipResourceFile final : public ResourceFile
{
public:
	unzFile m_archive = nullptr;
	PtrSize m_size = 0;

	~ZipResourceFile()
	{
		if(m_archive)
		{
			// It's open
			unzClose(m_archive);
			m_archive = nullptr;
			m_size = 0;
		}
	}

	Error open(const CString& archive, const CString& archivedFname)
	{
		// Open archive
		m_archive = unzOpen(&archive[0]);
		if(m_archive == nullptr)
		{
			ANKI_RESOURCE_LOGE("Failed to open archive");
			return Error::kFileAccess;
		}

		// Locate archived
		const int caseSensitive = 1;
		if(unzLocateFile(m_archive, &archivedFname[0], caseSensitive) != UNZ_OK)
		{
			ANKI_RESOURCE_LOGE("Failed to locate file in archive");
			return Error::kFileAccess;
		}

		// Open file
		if(unzOpenCurrentFile(m_archive) != UNZ_OK)
		{
			ANKI_RESOURCE_LOGE("unzOpenCurrentFile() failed");
			return Error::kFileAccess;
		}

		// Get size just in case
		unz_file_info zinfo;
		zinfo.uncompressed_size = 0;
		unzGetCurrentFileInfo(m_archive, &zinfo, nullptr, 0, nullptr, 0, nullptr, 0);
		m_size = zinfo.uncompressed_size;
		ANKI_ASSERT(m_size != 0);

		return Error::kNone;
	}

	void close()
	{
		if(m_archive)
		{
			unzClose(m_archive);
			m_archive = nullptr;
			m_size = 0;
		}
	}

	Error read(void* buff, PtrSize size) override
	{
		ANKI_TRACE_SCOPED_EVENT(RsrcFileRead);

		I64 readSize = unzReadCurrentFile(m_archive, buff, U32(size));

		if(I64(size) != readSize)
		{
			ANKI_RESOURCE_LOGE("File read failed");
			return Error::kFileAccess;
		}

		return Error::kNone;
	}

	Error readAllText(ResourceString& out) override
	{
		ANKI_ASSERT(m_size);
		out = ResourceString('?', m_size);
		return read(&out[0], m_size);
	}

	Error readU32(U32& u) override
	{
		// Assume machine and file have same endianness
		ANKI_CHECK(read(&u, sizeof(u)));
		return Error::kNone;
	}

	Error readF32(F32& u) override
	{
		// Assume machine and file have same endianness
		ANKI_CHECK(read(&u, sizeof(u)));
		return Error::kNone;
	}

	Error seek(PtrSize offset, FileSeekOrigin origin) override
	{
		// Rewind if needed
		if(origin == FileSeekOrigin::kBeginning)
		{
			if(unzCloseCurrentFile(m_archive) || unzOpenCurrentFile(m_archive))
			{
				ANKI_RESOURCE_LOGE("Rewind failed");
				return Error::kFunctionFailed;
			}
		}

		// Move forward by reading dummy data
		Array<char, 128> buff;
		while(offset != 0)
		{
			PtrSize toRead = min<PtrSize>(offset, sizeof(buff));
			ANKI_CHECK(read(&buff[0], toRead));
			offset -= toRead;
		}

		return Error::kNone;
	}

	PtrSize getSize() const override
	{
		ANKI_ASSERT(m_size > 0);
		return m_size;
	}
};

ResourceFilesystem::~ResourceFilesystem()
{
}

Error ResourceFilesystem::init()
{
	ResourceStringList paths;
	paths.splitString(g_dataPathsCVar, ':');

	// Workaround the fact that : is used in drives in Windows
#if ANKI_OS_WINDOWS
	ResourceStringList paths2;
	ResourceStringList::Iterator it = paths.getBegin();
	while(it != paths.getEnd())
	{
		const ResourceString& s = *it;
		ResourceStringList::Iterator it2 = it + 1;
		if(s.getLength() == 1 && (s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') && it2 != paths.getEnd())
		{
			paths2.pushBackSprintf("%s:%s", s.cstr(), it2->cstr());
			++it;
		}
		else
		{
			paths2.pushBack(s);
		}

		++it;
	}

	paths.destroy();
	paths = std::move(paths2);
#endif

	if(paths.getSize() < 1)
	{
		ANKI_RESOURCE_LOGE("Config option \"g_dataPathsCVar\" is empty");
		return Error::kUserData;
	}

	for(const auto& path : paths)
	{
		ResourceStringList includedStrings;
		ResourceStringList excludedStrings;
		ResourceString actualPath;
		ANKI_CHECK(tokenizePath(path, actualPath, includedStrings, excludedStrings));

		ANKI_CHECK(addNewPath(actualPath, includedStrings, excludedStrings));
	}

#if ANKI_OS_ANDROID
	// Add the external storage
	ANKI_CHECK(addNewPath(g_androidApp->activity->externalDataPath, {}, {}));

	// ...and then the apk assets
	ANKI_CHECK(addNewPath(".apk assets", {}, {}));
#endif

	return Error::kNone;
}

Error ResourceFilesystem::addNewPath(CString filepath, const ResourceStringList& includedStrings, const ResourceStringList& excludedStrings)
{
	ANKI_RESOURCE_LOGV("Adding new resource path: %s", filepath.cstr());

	U32 fileCount = 0; // Count files manually because it's slower to get that number from the list
	constexpr CString extension(".ankizip");

	auto includePath = [&](CString p) -> Bool {
		for(const ResourceString& s : excludedStrings)
		{
			const Bool found = p.find(s) != CString::kNpos;
			if(found)
			{
				return false;
			}
		}

		if(!includedStrings.isEmpty())
		{
			for(const ResourceString& s : includedStrings)
			{
				const Bool found = p.find(s) != CString::kNpos;
				if(found)
				{
					return true;
				}
			}

			return false;
		}

		return true;
	};

	PtrSize pos;
	Path path;
	if((pos = filepath.find(extension)) != CString::kNpos && pos == filepath.getLength() - extension.getLength())
	{
		// It's an archive

		// Open
		unzFile zfile = unzOpen(&filepath[0]);
		if(!zfile)
		{
			ANKI_RESOURCE_LOGE("Failed to open archive");
			return Error::kFileAccess;
		}

		// List files
		if(unzGoToFirstFile(zfile) != UNZ_OK)
		{
			unzClose(zfile);
			ANKI_RESOURCE_LOGE("unzGoToFirstFile() failed. Empty archive?");
			return Error::kFileAccess;
		}

		do
		{
			Array<char, 1024> filename;

			unz_file_info info;
			if(unzGetCurrentFileInfo(zfile, &info, &filename[0], filename.getSize(), nullptr, 0, nullptr, 0) != UNZ_OK)
			{
				unzClose(zfile);
				ANKI_RESOURCE_LOGE("unzGetCurrentFileInfo() failed");
				return Error::kFileAccess;
			}

			const Bool itsADir = info.uncompressed_size == 0;
			if(!itsADir && includePath(&filename[0]))
			{
				path.m_files.pushBackSprintf("%s", &filename[0]);
				++fileCount;
			}
		} while(unzGoToNextFile(zfile) == UNZ_OK);

		unzClose(zfile);

		path.m_isArchive = true;
	}
#if ANKI_OS_ANDROID
	else if(filepath == ".apk assets")
	{
		File dirStructureFile;
		ANKI_CHECK(dirStructureFile.open("DirStructure.txt", FileOpenFlag::kRead | FileOpenFlag::kSpecial));

		ResourceString fileTxt;
		ANKI_CHECK(dirStructureFile.readAllText(fileTxt));

		ResourceStringList lines;
		lines.splitString(fileTxt, '\n');

		for(const auto& line : lines)
		{
			if(includePath(line))
			{
				path.m_files.pushBack(line);
				++fileCount;
			}
		}

		path.m_isSpecial = true;
	}
#endif
	else
	{
		// It's simple directory

		ANKI_CHECK(walkDirectoryTree(filepath, [&](const CString& fname, Bool isDir) -> Error {
			if(!isDir && includePath(fname))
			{
				path.m_files.pushBackSprintf("%s", fname.cstr());
				++fileCount;
			}

			return Error::kNone;
		}));
	}

	ANKI_ASSERT(path.m_files.getSize() == fileCount);
	if(fileCount == 0)
	{
		ANKI_RESOURCE_LOGW("Ignoring empty resource path: %s", &filepath[0]);
	}
	else
	{
		path.m_path.sprintf("%s", &filepath[0]);
		m_paths.emplaceFront(std::move(path));

		ANKI_RESOURCE_LOGI("Added new data path \"%s\" that contains %u files", &filepath[0], fileCount);
	}

	if(false)
	{
		for(const ResourceString& s : m_paths.getFront().m_files)
		{
			printf("%s\n", s.cstr());
		}
	}

	return Error::kNone;
}

Error ResourceFilesystem::openFile(const ResourceFilename& filename, ResourceFilePtr& filePtr) const
{
	ResourceFile* rfile;
	Error err = openFileInternal(filename, rfile);

	if(err)
	{
		ANKI_RESOURCE_LOGE("Resource file not found: %s", filename.cstr());
		deleteInstance(ResourceMemoryPool::getSingleton(), rfile);
	}
	else
	{
		ANKI_ASSERT(rfile);
		filePtr.reset(rfile);
	}

	return err;
}

Error ResourceFilesystem::openFileInternal(const ResourceFilename& filename, ResourceFile*& rfile) const
{
	ANKI_RESOURCE_LOGV("Opening resource file: %s", filename.cstr());
	rfile = nullptr;

	// Search for the fname in reverse order
	for(const Path& p : m_paths)
	{
		for(const ResourceString& pfname : p.m_files)
		{
			if(pfname != filename)
			{
				continue;
			}

			// Found
			if(p.m_isArchive)
			{
				ZipResourceFile* file = newInstance<ZipResourceFile>(ResourceMemoryPool::getSingleton());
				rfile = file;

				ANKI_CHECK(file->open(p.m_path.toCString(), filename));
			}
			else
			{
				ResourceString newFname;
				if(!p.m_isSpecial)
				{
					newFname.sprintf("%s/%s", &p.m_path[0], &filename[0]);
				}
				else
				{
					newFname = filename;
				}

				CResourceFile* file = newInstance<CResourceFile>(ResourceMemoryPool::getSingleton());
				rfile = file;

				FileOpenFlag openFlags = FileOpenFlag::kRead;
				if(p.m_isSpecial)
				{
					openFlags |= FileOpenFlag::kSpecial;
				}

				ANKI_CHECK(file->m_file.open(newFname, openFlags));

#if 0
				printf("Opening asset %s\n", &newFname[0]);
#endif
			}
		}

		if(rfile)
		{
			break;
		}
	} // end for all paths

#if !ANKI_OS_ANDROID
	// File not found? On Win/Linux try to find it outside the resource dirs
	if(!rfile)
	{
		CResourceFile* file = newInstance<CResourceFile>(ResourceMemoryPool::getSingleton());
		rfile = file;
		ANKI_CHECK(file->m_file.open(filename, FileOpenFlag::kRead));
		ANKI_RESOURCE_LOGW("Loading resource outside the resource paths/archives. This is only OK for tools and debugging: %s", filename.cstr());
	}
#else
	if(!rfile)
	{
		ANKI_RESOURCE_LOGE("Couldn't find file: %s", filename.cstr());
		return Error::kFileNotFound;
	}
#endif

	return Error::kNone;
}

} // end namespace anki
