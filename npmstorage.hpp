#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/time.hpp>

#define EMPTY_PRERELEASE_SID 0xffffffff
#define EMPTY_PRERELEASE_FULL_SID 0xffffffff

#define DEBUG_CONTRACT_ADMIN "npmtesting11"_n

#define REPO_CONTRACT_ADMIN "npmtesting11"_n

#define REPO_TYPE_COMMUNITY 1
#define REPO_TYPE_OFFICIAL 0xffffffff

#define LOAD_ORDER_STRICT 0
#define LOAD_ORDER_ANY 1

#define FILE_FORMAT_JS 1
#define FILE_FORMAT_CSS 2
#define FILE_MODE_STANDARD 1
#define FILE_MODE_INJECT_INLINE 2

#define FILE_TYPE_STANDARD_JS (FILE_MODE_STANDARD<<8 | FILE_FORMAT_JS)
#define FILE_TYPE_STANDARD_CSS (FILE_MODE_STANDARD<<8 | FILE_FORMAT_CSS)
#define FILE_TYPE_INJECT_INLINE_JS (FILE_MODE_INJECT_INLINE<<8 | FILE_FORMAT_JS)
#define FILE_TYPE_INJECT_INLINE_CSS (FILE_MODE_INJECT_INLINE<<8 | FILE_FORMAT_CSS)


#define RELEASE_STATUS_DISABLED 0
#define RELEASE_STATUS_ACTIVE 1

#define get_rloadindex(release_id, load_index) \
  (eosio::checksum256::make_from_word_sequence<uint64_t>((uint64_t)0, (uint64_t)0, release_id,(uint64_t)load_index))
#define is_valid_file_type(file_type) \
  (file_type == FILE_TYPE_STANDARD_JS || file_type == FILE_TYPE_STANDARD_CSS || file_type == FILE_TYPE_INJECT_INLINE_JS || file_type == FILE_TYPE_INJECT_INLINE_CSS)

using namespace eosio;

struct semver_version_number_t {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;

};
struct parsed_package_version_t {
  std::string package_and_version;
  std::string package_name;
  std::string version_base;
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  std::string prerelease;
  std::string prerelease_full;
};

bool validate_resource_file_alt_sources(std::string alt_sources){
  return true;
}

bool validate_resource_file_externals(std::string externals){
  return true;
}

uint32_t parse_semver_version(std::string version_string, semver_version_number_t &result){
  std::size_t version_string_length = version_string.length();
  int zz = 2;
  if(version_string_length==0){
    return 1;
  }
  int t = 2;
  std::size_t pos = 0;

  std::size_t dot_pos_1 = version_string.find('.');
  if(dot_pos_1 == std::string::npos){
    return 1;
  }else if(dot_pos_1 == 0){
    return 2;
  }
  

  std::size_t dot_pos_2 = version_string.find('.', dot_pos_1+1);
  if(dot_pos_2 == std::string::npos){
    return 3;
  }else if(dot_pos_2 == dot_pos_1+1){
    return 4;
  }else if(dot_pos_2 == (version_string_length-1)){
    return 5;
  }

  std::string major_string = version_string.substr(0, dot_pos_1);
  uint32_t major = std::stoul(major_string);
  if(major_string.compare(std::to_string(major)) != 0){
    return 6;
  }
  //eosio::check(major_string.compare(std::to_string(major)) == 0, "error parsing major part of version");


  std::string minor_string = version_string.substr(dot_pos_1+1, dot_pos_2-(dot_pos_1+1));
  uint32_t minor = std::stoul(minor_string);

  if(minor_string.compare(std::to_string(minor)) != 0){
    return 7;
  }
  //eosio::check(minor_string.compare(std::to_string(minor)) == 0, "error parsing minor part of version");


  std::string patch_string = version_string.substr(dot_pos_2+1);
  uint32_t patch = std::stoul(patch_string);
  if(patch_string.compare(std::to_string(patch)) != 0){
    return 8;
  }
  //eosio::check(patch_string.compare(std::to_string(patch)) == 0, "error parsing patch part of version");

  result.major = major;
  result.minor = minor;
  result.patch = patch;
  return 0;
}
void parse_package_and_version(std::string package_and_version_raw, parsed_package_version_t &result) {
  std::size_t pkg_version_raw_length = package_and_version_raw.length();
  // minimum package_and_version_raw should be 7 as "a@0.0.0".length == 7
  eosio::check(pkg_version_raw_length >= 7, "package_and_version_raw has invalid length (<7)");




  std::size_t version_at_pos = package_and_version_raw.find('@', package_and_version_raw.at(0) == '@' ? 1 : 0);
  eosio::check(version_at_pos != std::string::npos, "package_and_version_raw missing @ symbol");
  eosio::check(version_at_pos != (pkg_version_raw_length-1), "package_and_version_raw missing version");
  
  std::string package_name = package_and_version_raw.substr(0, version_at_pos);
  std::string version_full = package_and_version_raw.substr(version_at_pos+1);

  std::size_t version_dash_pos = version_full.find('-');
  
  std::string prerelease;
  std::string prerelease_full;
  std::string version_base;

  if(version_dash_pos == std::string::npos){
    version_base = version_full;
    prerelease_full = "";
    prerelease = "";
  }else{
    version_base = version_full.substr(0, version_dash_pos);
    prerelease_full = version_full.substr(version_dash_pos+1);
    std::size_t prerelease_dot_pos = prerelease_full.find('.');
    if(prerelease_dot_pos == std::string::npos){
      prerelease = prerelease_full;
    }else{
      eosio::check(prerelease_dot_pos != 0, "prerelease_full should not start with a dot!");
      eosio::check(prerelease_dot_pos != prerelease_full.length()-1, "prerelease_full should end with a dot!");
      prerelease = prerelease_full.substr(0, prerelease_dot_pos);
    }
  }
  semver_version_number_t semver;
  eosio::check(parse_semver_version(version_base, semver) == 0, "error parsing semver version_base!");

  result.package_and_version = package_and_version_raw;
  result.package_name = package_name;

  result.version_base = version_base;
  result.major = semver.major;
  result.minor = semver.minor;
  result.patch = semver.patch;

  result.prerelease = prerelease;
  result.prerelease_full = prerelease_full;

}
checksum256 get_package_version_combined(uint32_t package_name_id, uint32_t major, uint32_t minor, uint32_t patch, uint32_t prerelease_sid, uint32_t prerelease_full_sid) {
  checksum256 combined = eosio::checksum256::make_from_word_sequence<uint32_t>(package_name_id,major,minor,patch,prerelease_sid,(uint32_t)0,(uint32_t)0,prerelease_full_sid);
  return combined;
}


CONTRACT npmstorage : public contract {
  public:
    using contract::contract;
    npmstorage( name receiver, name code, datastream<const char*> ds )
        : contract(receiver, code, ds),
          tbl_stringstore(receiver, receiver.value),
          tbl_packagenames(receiver, receiver.value),
          tbl_packageversions(receiver, receiver.value),
          tbl_repos(receiver, receiver.value),
          tbl_releases(receiver, receiver.value),
          tbl_releasefiles(receiver, receiver.value),
          tbl_resources(receiver, receiver.value) {}

    
    // ACTION addpkgver(name user, std::string package_and_version);
    ACTION upsertrepo(name user, name repo, std::string title, std::string description, std::string url, std::string icon);
    
    ACTION addrelease(name user, name repo, std::string package_and_version, uint32_t load_order);
    ACTION delrelease(name user, uint64_t release_id);
    
    ACTION setreleaseon(name user, uint64_t release_id, uint32_t status);
    ACTION setloadorder(name user, uint64_t release_id, uint32_t load_order);
    ACTION addrelfile(name user, uint64_t release_id, checksum256 filehash, std::string alt_sources, std::string externals, uint32_t file_type, uint32_t load_index);
    ACTION swaploadind(name user, uint64_t release_id, uint64_t releasefile_id_a, uint64_t releasefile_id_b);

    ACTION add(name uploader, checksum256 sha256hash, std::string data);
    
    
    
    
    
    ACTION devclearall();

    TABLE s_tbl_stringstore {
      uint32_t id;
      std::string value;
      checksum256 value_hash;
      uint64_t primary_key()const { return id; }
      checksum256 by_hash()const { return value_hash; }
    };

    TABLE s_tbl_packagenames {
      uint32_t id;
      std::string package_name;
      checksum256 package_name_hash;

      uint64_t primary_key()const { return id; }
      checksum256 by_hash()const { return package_name_hash; }
    };


    TABLE s_tbl_packageversions {
      uint64_t id;
      std::string package_and_version;
      checksum256 package_and_version_hash;
      
      std::string package_name;
      uint32_t package_name_id;


      std::string version_base;
      uint32_t major;
      uint32_t minor;
      uint32_t patch;

      std::string prerelease;
      uint32_t prerelease_sid;

      std::string prerelease_full;
      uint32_t prerelease_full_sid;


      checksum256 combined; 

      name creator;

      uint64_t num_releases;


      uint64_t primary_key()const { return id; }
      checksum256 by_hash()const { return package_and_version_hash; }
      checksum256 by_combined()const { return combined; }
      uint64_t by_creator()const { return creator.value; }
    };

    TABLE s_tbl_repos {
      name repo;
      name owner;
      std::string title;
      std::string description;
      std::string url;
      std::string icon;
      uint32_t status;

      uint64_t primary_key()const { return repo.value; }
      uint64_t by_status()const { return status; }
    };

    TABLE s_tbl_releases {
      uint64_t id;
      
      name repo;
      uint32_t package_name_id;
      uint64_t package_version_id;
      std::string package_and_version;

      uint32_t file_count;
      uint32_t load_order;

      uint32_t status;
      uint64_t created_at;
      
      uint64_t primary_key()const { return id; }
      uint64_t by_pkg_version()const { return package_version_id; }
      checksum256 by_repo_pkgver()const { return eosio::checksum256::make_from_word_sequence<uint64_t>(repo.value, (uint64_t)package_name_id, package_version_id,(uint64_t)0); }
    };



    TABLE s_tbl_releasefiles {
      uint64_t id;
      uint64_t release_id;
      uint64_t package_version_id;

      name repo;
      uint32_t load_index;
      uint32_t file_type;
      
      checksum256 sha256hash;
      uint64_t resource_id;

      std::string alt_sources;
      std::string externals;
      checksum256 rloadindex;

      uint64_t primary_key()const { return id; }
      checksum256 by_rloadindex()const { return rloadindex; }
      uint64_t by_release_id()const { return release_id; }
      checksum256 by_hash()const { return sha256hash; }
    };

    TABLE s_tbl_resources {
      uint64_t rid;
      name uploader;
      checksum256 sha256hash;
      std::string data;
      uint64_t primary_key()const { return rid; }
      checksum256 by_hash()const { return sha256hash; }
    };

    typedef eosio::multi_index<"stringstore"_n, s_tbl_stringstore, 
      eosio::indexed_by<"byhash"_n, eosio::const_mem_fun<s_tbl_stringstore, checksum256, &s_tbl_stringstore::by_hash> >
    > t_tbl_stringstore;


    typedef eosio::multi_index<"pkgnames"_n, s_tbl_packagenames, 
      eosio::indexed_by<"byhash"_n, eosio::const_mem_fun<s_tbl_packagenames, checksum256, &s_tbl_packagenames::by_hash> >
    > t_tbl_packagenames;


    typedef eosio::multi_index<"pkgversions"_n, s_tbl_packageversions, 
      eosio::indexed_by<"byhash"_n, eosio::const_mem_fun<s_tbl_packageversions, checksum256, &s_tbl_packageversions::by_hash> >,
      eosio::indexed_by<"bycombined"_n, eosio::const_mem_fun<s_tbl_packageversions, checksum256, &s_tbl_packageversions::by_combined> >,
      eosio::indexed_by<"bycreator"_n, eosio::const_mem_fun<s_tbl_packageversions, uint64_t, &s_tbl_packageversions::by_creator> >
    > t_tbl_packageversions;



    typedef eosio::multi_index<"repos"_n, s_tbl_repos, 
      eosio::indexed_by<"bystatus"_n, eosio::const_mem_fun<s_tbl_repos, uint64_t, &s_tbl_repos::by_status> >
    > t_tbl_repos;

    
    typedef eosio::multi_index<"releases"_n, s_tbl_releases, 
      eosio::indexed_by<"bypkgversion"_n, eosio::const_mem_fun<s_tbl_releases, uint64_t, &s_tbl_releases::by_pkg_version> >,
      eosio::indexed_by<"byrepopkgver"_n, eosio::const_mem_fun<s_tbl_releases, checksum256, &s_tbl_releases::by_repo_pkgver> >
    > t_tbl_releases;


    typedef eosio::multi_index<"releasefiles"_n, s_tbl_releasefiles, 
      eosio::indexed_by<"byrloadindex"_n, eosio::const_mem_fun<s_tbl_releasefiles, checksum256, &s_tbl_releasefiles::by_rloadindex> >,
      eosio::indexed_by<"byreleaseid"_n, eosio::const_mem_fun<s_tbl_releasefiles, uint64_t, &s_tbl_releasefiles::by_release_id> >,
      eosio::indexed_by<"byhash"_n, eosio::const_mem_fun<s_tbl_releasefiles, checksum256, &s_tbl_releasefiles::by_hash> >
    > t_tbl_releasefiles;

    typedef eosio::multi_index<"resources"_n, s_tbl_resources, 
      eosio::indexed_by<"datahashidx"_n, eosio::const_mem_fun<s_tbl_resources, checksum256, &s_tbl_resources::by_hash> >
    > t_tbl_resources;





    //using addpkgver_action = action_wrapper<"addpkgver"_n, &npmstorage::addpkgver>;
    using upsertrepo_action = action_wrapper<"upsertrepo"_n, &npmstorage::upsertrepo>;
    using addrelease_action = action_wrapper<"addrelease"_n, &npmstorage::addrelease>;
    using delrelease_action = action_wrapper<"delrelease"_n, &npmstorage::delrelease>;
    using setreleaseon_action = action_wrapper<"setreleaseon"_n, &npmstorage::setreleaseon>;
    using setloadorder_action = action_wrapper<"setloadorder"_n, &npmstorage::setloadorder>;
    using addrelfile_action = action_wrapper<"addrelfile"_n, &npmstorage::addrelfile>;
    using swaploadind_action = action_wrapper<"swaploadind"_n, &npmstorage::swaploadind>;
    using add_action = action_wrapper<"add"_n, &npmstorage::add>;

    using devclearall_action = action_wrapper<"devclearall"_n, &npmstorage::devclearall>;
    
/*

    ACTION addpkgver(name user, std::string package_and_version);
    ACTION upsertrepo(name user, name repo, std::string title, std::string description, std::string url, std::string icon);
    
    ACTION addrelease(name user, name repo, std::string package_and_version);
    ACTION delrelease(name user, uint64_t release_id);
    
    ACTION setreleaseon(name user, uint64_t release_id, uint32_t active);
    ACTION addrelfile(name user, uint64_t release_id, checksum256 filehash, std::string alt_sources, std::string externals);
    ACTION swaploadind(name user, uint64_t release_id, uint64_t releasefile_id_a, uint64_t releasefile_id_b);

    ACTION add(name uploader, checksum256 sha256hash, std::string data);
*/

    t_tbl_stringstore tbl_stringstore;
    t_tbl_packagenames tbl_packagenames;
    t_tbl_packageversions tbl_packageversions;

    t_tbl_repos tbl_repos;
    t_tbl_releases tbl_releases;
    t_tbl_releasefiles tbl_releasefiles;

    t_tbl_resources tbl_resources;
    

  private:
    uint32_t add_value_to_stringstore(name user, std::string value, bool error_if_exists);
    uint32_t add_value_to_packagenames(name user, std::string package_name, bool error_if_exists);
    uint64_t add_package_version(name user, std::string package_and_version, bool error_if_exists);
    bool claim_repo(name user, name repo);
    auto assert_can_upsert_repo(name user, name repo);
    auto assert_user_owns_repo(name user, name repo);
    auto assert_user_owns_release(name user, uint64_t release_id);
    void upsert_repo_content(name user, name repo, std::string title, std::string description, std::string url, std::string icon);
    void add_new_release(name user, name repo, std::string package_and_version, uint32_t load_order);
    void set_release_status(name user, uint64_t release_id, uint32_t status);
    void set_release_load_order(name user, uint64_t release_id, uint32_t load_order);
    void add_release_file(name user, uint64_t release_id, checksum256 filehash, std::string alt_sources, std::string externals, uint32_t file_type, uint32_t load_index);





};
