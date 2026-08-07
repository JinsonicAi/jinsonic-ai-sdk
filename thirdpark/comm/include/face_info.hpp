#ifndef __FACE_INFO_HPP__
#define __FACE_INFO_HPP__
#include <cstring>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "DevProtoDef.hpp"

struct FaceRecord {
	int				   id;
	std::string		   name;
	std::vector<float> feature;	 //  use the float feature
	std::string		   img_path;
	int				   gender;
	int				   age;
	std::string		   phone;
	// 保留旧字段供现有调用方兼容；人员类别判断应使用 person_type。
	bool			   widelist{false};
	PersonType		   person_type{PersonType::BLACKLIST};
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
					stranger(false),
					empty_db(false),
					uncertain(false),
						person_type(PersonType::NONE) {}
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
	bool	 empty_db;	 // true when face DB is empty (no registered faces)
	bool	 uncertain;	 // true when score falls in gray zone
	PersonType person_type;
};

// int loadFacesFromDatabase(int folder_id, std::vector<FaceRecord>& out_faces);
class FaceRecognizer {
public:
	FaceRecognizer(const std::string& folder_id);

	// enter features and return matching results
	MatchResult recognize(float feature[512], float threshold = 0.95f, float gray_zone = 0.0f, float quality = 1.0f) const;

	int						 size();
	std::vector<FaceRecord>& getFaceDB();

private:
	void load_faces(bool force);
	void reload_if_changed() const;

	std::string				folder_id_{};
	mutable std::vector<FaceRecord> face_db_;
	mutable std::mutex		face_db_mutex_;
	mutable long long		db_stamp_{-1};
	mutable long long		wal_stamp_{-1};
};

#endif
