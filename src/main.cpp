#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cpr/cpr.h>
#include <regex>
#include <nlohmann/json.hpp>
#include <Python.h>



using json = nlohmann::json;


// Helper to convert C++ string → PyObject*
static PyObject* PyStr(const std::string& s) {
    return PyUnicode_FromStringAndSize(s.c_str(), s.size());
}

// A quick helper to trim whitespace
static std::string trim(const std::string &s)
{
   auto wsfront = std::find_if_not(s.begin(), s.end(),
                                   [](unsigned char c)
                                   { return std::isspace(c); });
   auto wsback = std::find_if_not(s.rbegin(), s.rend(),
                                  [](unsigned char c)
                                  { return std::isspace(c); })
                     .base();
   return (wsback <= wsfront ? std::string()
                             : std::string(wsfront, wsback));
}

bool is_valid_playlist(const std::string &link)
{
   cpr::Response r = cpr::Get(
       cpr::Url{link},
       cpr::Header{
           {"User-Agent",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/91.0.4472.124 Safari/537.36"},
           {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
           {"Accept-Language", "en-US,en;q=0.5"}});
   std::cout << "DEBUG: is_valid_playlist - Status code: "
             << r.status_code << "\n"
             << "DEBUG: is_valid_playlist - Response size: "
             << r.text.size() << " bytes\n";
   if (r.status_code >= 200 && r.status_code < 300)
   {
      std::cerr << "Successfully fetched the playlist.\n";
      return true;
   }
   else
   {
      std::cerr << "Error fetching the playlist: "
                << r.status_code << "\n"
                << "Error details: " << r.error.message << "\n";
      return false;
   }
}

json extract_songs_and_artists(const std::string &link)
{
   std::cout << "DEBUG: Starting extract_songs_and_artists for: " << link << "\n";

   // 1) Fetch the page
   cpr::Response r = cpr::Get(
       cpr::Url{link},
       cpr::Header{
           {"User-Agent",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/91.0.4472.124 Safari/537.36"},
           {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
           {"Accept-Language", "en-US,en;q=0.5"}});
   std::cout << "DEBUG: HTTP Status Code: " << r.status_code
             << ", Response size: " << r.text.size() << " bytes\n";
   if (r.status_code != 200 || r.text.empty())
   {
      std::cerr << "Error fetching playlist page or empty body\n";
      return json::array();
   }
   const std::string &html = r.text;

   // 2) Locate <script … id="serialized-server-data"> … </script>
   size_t start = std::string::npos;
   {
      size_t cursor = html.find("<script");
      while (cursor != std::string::npos)
      {
         size_t tag_end = html.find('>', cursor);
         if (tag_end == std::string::npos)
            break;
         std::string opening = html.substr(cursor, tag_end - cursor + 1);
         if (opening.find(R"(id="serialized-server-data")") != std::string::npos)
         {
            start = tag_end + 1;
            break;
         }
         cursor = html.find("<script", tag_end);
      }
   }
   if (start == std::string::npos)
   {
      std::cerr << "No serialized-server-data <script> tag found\n";
      return json::array();
   }
   size_t end = html.find("</script>", start);
   if (end == std::string::npos)
   {
      std::cerr << "No closing </script> found\n";
      return json::array();
   }
   std::string embedded_json = html.substr(start, end - start);
   std::cout << "DEBUG: Extracted JSON length = " << embedded_json.size()
             << ", snippet = " << embedded_json.substr(0, 200) << "...\n";

   // 3) Parse JSON and extract songs with three fallbacks
   json song_list = json::array();
   try
   {
      json outer = json::parse(embedded_json);
      if (!outer.is_array() || outer.empty())
      {
         std::cerr << "Top-level JSON not an array or is empty\n";
         return song_list;
      }
      const auto &data = outer[0]["data"];

      // Fallback #1: data.seoData.ogSongs
      if (data.contains("seoData") && data["seoData"].contains("ogSongs") && data["seoData"]["ogSongs"].is_array())
      {

         const auto &og = data["seoData"]["ogSongs"];
         std::cerr << "DEBUG: Using seoData.ogSongs (" << og.size() << ")\n";
         for (const auto &song : og)
         {
            const auto &a = song["attributes"];
            if (a.contains("artistName") && a.contains("name"))
            {
               song_list.push_back({{"Artist", a["artistName"]},
                                    {"Song", a["name"]}});
            }
         }
      }
      // Fallback #2: data.playlist.tracks.data
      else if (data.contains("playlist") && data["playlist"].contains("tracks") && data["playlist"]["tracks"].contains("data") && data["playlist"]["tracks"]["data"].is_array())
      {

         const auto &arr = data["playlist"]["tracks"]["data"];
         std::cerr << "DEBUG: Using playlist.tracks.data (" << arr.size() << ")\n";
         for (const auto &song : arr)
         {
            const auto &a = song["attributes"];
            if (a.contains("artistName") && a.contains("name"))
            {
               song_list.push_back({{"Artist", a["artistName"]},
                                    {"Song", a["name"]}});
            }
         }
      }
      // Fallback #3: data.sections → id contains "track-list" → items
      else if (data.contains("sections") && data["sections"].is_array())
      {
         std::cerr << "DEBUG: Using sections → track-list\n";
         for (const auto &section : data["sections"])
         {
            if (section.contains("id") && section["id"].get<std::string>().find("track-list") != std::string::npos && section.contains("items") && section["items"].is_array())
            {

               for (const auto &song : section["items"])
               {
                  std::string title = song.value("title", "");
                  std::string artists;
                  if (song.contains("subtitleLinks"))
                  {
                     for (const auto &link : song["subtitleLinks"])
                     {
                        if (link.contains("title"))
                        {
                           if (!artists.empty())
                              artists += " & ";
                           artists += link["title"].get<std::string>();
                        }
                     }
                  }
                  if (!title.empty() && !artists.empty())
                  {
                     song_list.push_back({{"Artist", artists},
                                          {"Song", title}});
                  }
               }
               break;
            }
         }
      }
      else
      {
         std::cerr << "DEBUG: No known song array found in JSON\n";
      }
   }
   catch (const json::parse_error &e)
   {
      std::cerr << "JSON parse error: " << e.what() << "\n";
   }
   catch (const json::exception &e)
   {
      std::cerr << "JSON access error: " << e.what() << "\n";
   }

   std::cerr << "DEBUG: Final song_list size: " << song_list.size() << "\n";
   return song_list;
}

void youtube_downloader(const json& result, const std::string& outtmpl) {
    // 1) Import our Python module
    PyObject* pName   = PyUnicode_FromString("downloader");
    PyObject* pModule = PyImport_Import(pName);
    Py_DECREF(pName);
    if (!pModule) {
        PyErr_Print();
        std::cerr << "Failed to import downloader.py\n";
        return;
    }

    // 2) Get the download_track() function
    PyObject* pFunc = PyObject_GetAttrString(pModule, "download_track");
    if (!pFunc || !PyCallable_Check(pFunc)) {
        std::cerr << "Cannot find function download_track\n";
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
        return;
    }

    // 3) Loop over each JSON entry
    for (const auto& entry : result) {
        std::string artist = entry.value("Artist", "");
        std::string song   = entry.value("Song", "");
        if (artist.empty() || song.empty()) continue;

        std::string query = "ytsearch1:" + artist + " - " + song;
        std::cout << ">> Downloading: " << artist << " – " << song
                  << " via [" << query << "]\n";

        PyObject* pArgs = PyTuple_Pack(
            2,
            PyStr(query),
            PyStr(outtmpl)
        );
        if (!pArgs) {
            PyErr_Print();
            continue;
        }

        PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
        Py_DECREF(pArgs);
        if (!pValue) {
            PyErr_Print();
            std::cerr << "Error calling download_track for " << query << "\n";
            continue;
        }

        // Print returned info dict
        PyObject* pRepr = PyObject_Repr(pValue);
        PyObject* pBytes= PyUnicode_AsEncodedString(pRepr, "utf-8", "strict");
        if (pBytes) {
            std::cout << "Track info: "
                      << PyBytes_AS_STRING(pBytes) << "\n";
            Py_DECREF(pBytes);
        }
        Py_DECREF(pRepr);
        Py_DECREF(pValue);
    }

    Py_DECREF(pFunc);
    Py_DECREF(pModule);
}

int main() {
    // Initialize Python
    Py_Initialize();
    PyRun_SimpleString("import sys\n"
                       "sys.path.insert(0, '')\n");

    // Ask for Apple Music link
    std::string link;
    const std::regex apple_music_regex(
        R"(https:\/\/music\.apple\.com\/.{2}\/playlist\/.+\/.+)"
    );
    while (true) {
        std::cout << "Please type in the link to your Apple Music Playlist:\n";
        std::getline(std::cin, link);
        link = trim(link);
        if (link == "Q" || link == "q") {
            std::cout << "Exiting...\n";
            Py_Finalize();
            return 0;
        }
        if (std::regex_match(link, apple_music_regex)) break;
        std::cout << "Invalid link. Try again or Q to quit.\n";
    }

    if (!is_valid_playlist(link)) {
        Py_Finalize();
        return 1;
    }

    json result = extract_songs_and_artists(link);
    std::cout << "Final result:\n" << result.dump(4) << "\n";

    // Download via Python/yt-dlp, saving under downloads/
    youtube_downloader(result, "downloads/%(title)s.%(ext)s");

    // Clean up Python
    Py_Finalize();
    return 0;
}