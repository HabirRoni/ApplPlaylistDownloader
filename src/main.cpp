#include <stdio.h>
#include <iostream>
#include <regex>
#include <string>
#include <algorithm>
#include <fmt/base.h>






int main() { 
   std::string link;
   bool valid_input = false;


   // Regular expression to match Apple Music playlist links
   std::regex apple_music_regex(R"(https:\/\/music\.apple\.com\/[a-z]{2}\/playlist\/[a-zA-Z0-9_-]+\/[0-9]+)");
   
   while (valid_input) {
      std::cout << "Please type in the link to your Apple Music PlaylistL: " << std::endl;
      getline(std::cin,link);
      if (link == "Q" || link == "q") {
         std::cout << "Exiting..." << std::endl;
         return 0;
      }
      // Trim Whitespace
      link.erase(link.begin(), std::find_if(link.begin(), link.end(), [](unsigned char ch) {
         return !std::isspace(ch);
      }));
      link.erase(std::find_if(link.rbegin(), link.rend(), [](unsigned char ch) {
         return !std::isspace(ch);
      }).base(), link.end());
      
      if (std::regex_match(link, apple_music_regex)) {
         valid_input = true;
      } else {
         std::cout << "Invalid link. Please try again, or to quit please Q." << std::endl;
      } 
      


   }
   std::cout << "Valid Apple Music Playlist link: " << link << std::endl;


}



