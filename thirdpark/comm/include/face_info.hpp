#ifndef __FACE_INFO_HPP__
#define __FACE_INFO_HPP__
#include <cstring>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <opencv2/core/core.hpp>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct FaceRecord {
	int				   id;
	std::string		   name;
	std::vector<float> feature;	 //  use the float feature
	std::string		   img_path;
	int				   gender;
	int				   age;
	std::string		   phone;
	bool			   widelist;
	std::string		   pid;
	std::string		   work_id;
	std::string		   id_card_no;
	std::string		   ic_card_no;
	std::string		   department;
};

struct MatchResult {
public:
	MatchResult() : matched(false),
					score(0.0f),
					id(-1),
					name(""),
					img_path(""),
					gender(0),
					age(0),
					phone(""),
					pid(""),
					work_id(""),
					id_card_no(""),
					ic_card_no(""),
					department(""),
					widelist(false),
					face_rect(cv::Rect()),
					stranger(false) {}
	bool  matched;
	float score;
	//
	int			id;
	std::string name;
	std::string img_path;
	int			gender;
	int			age;
	std::string phone;
	std::string pid;
	std::string work_id;
	std::string id_card_no;
	std::string ic_card_no;
	std::string department;
	// bool		blacklist;
	bool widelist;	// added whether it s a whitelist or not
	//
	cv::Rect face_rect;	 // new face frame coordinates
	bool	 stranger;	 // added whether it s a stranger or not
};

// int loadFacesFromDatabase(int folder_id, std::vector<FaceRecord>& out_faces);
class FaceRecognizer {
public:
	FaceRecognizer(const std::string& folder_id);

	// enter features and return matching results
	MatchResult recognize(float feature[512], float threshold = 0.95f) const;

	int						 size();
	std::vector<FaceRecord>& getFaceDB();

private:
	std::string				folder_id_{};
	std::vector<FaceRecord> face_db_;
};

#endif