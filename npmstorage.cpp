#include <npmstorage.hpp>



uint32_t npmstorage::add_value_to_stringstore(name user, std::string value, bool error_if_exists){
  checksum256 value_hash = sha256(value.c_str(), value.length());
  
  auto stringstore_hash_index = tbl_stringstore.get_index<"byhash"_n>();
  auto stringstore_hash_iterator = stringstore_hash_index.find(value_hash);

  if(stringstore_hash_iterator == stringstore_hash_index.end()){
    uint64_t new_id_64 = tbl_stringstore.available_primary_key();
    eosio::check(new_id_64 < 0xffffffff, "string store is full (new id overflow)!");
    uint32_t new_id = (uint32_t)(new_id_64);

    tbl_stringstore.emplace(user, [&](auto &row) {
      row.id = new_id;
      row.value = value;
      row.value_hash = value_hash;
    });
    return new_id;
  }else{
    eosio::check(error_if_exists == false, "this value already exists in stringstore");
    
    return stringstore_hash_iterator->id;
  }
}

uint32_t npmstorage::add_value_to_packagenames(name user, std::string package_name, bool error_if_exists){
  checksum256 package_name_hash = sha256(package_name.c_str(), package_name.length());
  
  auto packagenames_hash_index = tbl_packagenames.get_index<"byhash"_n>();
  auto packagenames_hash_iterator = packagenames_hash_index.find(package_name_hash);

  if(packagenames_hash_iterator == packagenames_hash_index.end()){
    uint64_t new_id_64 = tbl_packagenames.available_primary_key();
    eosio::check(new_id_64 < 0xffffffff, "package names is full (new id overflow)!");
    uint32_t new_id = (uint32_t)(new_id_64);

    tbl_packagenames.emplace(user, [&](auto &row) {
      row.id = new_id;
      row.package_name = package_name;
      row.package_name_hash = package_name_hash;
    });

    return new_id;
  }else{
    eosio::check(error_if_exists == false, "this package_name already exists in packagenames");
    
    return packagenames_hash_iterator->id;
  }
}

uint64_t npmstorage::add_package_version(name user, std::string package_and_version, bool error_if_exists){
  eosio::check(package_and_version.length() >= 7, "package_and_version must be at least of length 7 (ex. a@0.0.0)!");

  checksum256 package_and_version_hash = sha256(package_and_version.c_str(), package_and_version.length());
  auto packageversions_hash_index = tbl_packageversions.get_index<"byhash"_n>();
  auto packageversions_hash_iterator = packageversions_hash_index.find(package_and_version_hash);

  if(packageversions_hash_iterator == packageversions_hash_index.end()){


    parsed_package_version_t parsed_package_version;
    parse_package_and_version(package_and_version, parsed_package_version);

    uint32_t package_name_id = add_value_to_packagenames(user, parsed_package_version.package_name, false);
    uint32_t prerelease_sid = parsed_package_version.prerelease.empty()?EMPTY_PRERELEASE_SID:add_value_to_stringstore(user, parsed_package_version.prerelease, false);
    uint32_t prerelease_full_sid = parsed_package_version.prerelease_full.empty()?EMPTY_PRERELEASE_FULL_SID:add_value_to_stringstore(user, parsed_package_version.prerelease_full, false);

    checksum256 combined = get_package_version_combined(
      package_name_id,
      parsed_package_version.major,
      parsed_package_version.minor,
      parsed_package_version.patch,
      prerelease_sid,
      prerelease_full_sid
    );

    uint64_t new_id = tbl_packageversions.available_primary_key();

    tbl_packageversions.emplace(user, [&](auto &row) {
      row.id = new_id;
      row.package_and_version = parsed_package_version.package_and_version;
      row.package_and_version_hash = package_and_version_hash;
      
      row.package_name = parsed_package_version.package_name;
      row.package_name_id = package_name_id;


      row.version_base = parsed_package_version.version_base;
      row.major = parsed_package_version.major;
      row.minor = parsed_package_version.minor;
      row.patch = parsed_package_version.patch;

      row.prerelease = parsed_package_version.prerelease;
      row.prerelease_sid = prerelease_sid;

      row.prerelease_full = parsed_package_version.prerelease_full;
      row.prerelease_full_sid = prerelease_full_sid;
      row.combined = combined;
      row.creator = user;
      row.num_releases = 0;
    });

    return new_id;
  }else{
    eosio::check(error_if_exists == false, "this package_and_version already exists in packageversions!");

    return packageversions_hash_iterator->id;
  }
}



bool npmstorage::claim_repo(name user, name repo) {
  if(repo.length() == 12){
    eosio::check(user.value == repo.value,
      "new repos' names must be the same as the claiming user's eos name");
    return true;
  }else{
    // TODO: add claiming logic for special repos
    return user.value == REPO_CONTRACT_ADMIN.value;
  }
}
auto npmstorage::assert_can_upsert_repo(name user, name repo) {
  auto repos_iterator = tbl_repos.find(repo.value);
  if(repos_iterator == tbl_repos.end()){
    eosio::check(claim_repo(user, repo) == true,
      "user does not have the right to claim this repo");
  }else{
    name owner = repos_iterator->owner;
    eosio::check(owner.value == user.value,
      "user does not own this repo");
  }
  return repos_iterator;
}

auto npmstorage::assert_user_owns_repo(name user, name repo) {
  auto repos_iterator = tbl_repos.find(repo.value);
  eosio::check(repos_iterator != tbl_repos.end(), "repo does not exist!");
  eosio::check((repos_iterator->owner).value == user.value, "user does not own this repo!");
  return repos_iterator;
}

auto npmstorage::assert_user_owns_release(name user, uint64_t release_id) {
  auto releases_iterator = tbl_releases.find(release_id);
  eosio::check(releases_iterator != tbl_releases.end(), "release does not exist!");
  assert_user_owns_repo(user, releases_iterator->repo);
  return releases_iterator;
}
void npmstorage::upsert_repo_content(name user, name repo, std::string title, std::string description, std::string url, std::string icon) {
  
  eosio::check(title.length() <= 128 && title.length() > 0, "title should not be empty and must be less <= 128 characters long");
  eosio::check(description.length() <= 1024, "description length must be <= 1024 characters long");
  eosio::check(url.length() <= 2048 , "url length must be <= 2048 characters long");
  eosio::check(icon.length() <= 2048 , "icon length must be <= 2048 characters long");
  
  auto repos_iterator = assert_can_upsert_repo(user, repo);

  if(repos_iterator == tbl_repos.end()){
    tbl_repos.emplace(user, [&](auto &row) {
      row.repo = repo;
      row.owner = user;
      row.title = title;
      row.description = description;
      row.url = url;
      row.icon = icon;
    });
  }else{
    tbl_repos.modify(repos_iterator, user, [&](auto &row) {
      row.title = title;
      row.description = description;
      row.url = url;
      row.icon = icon;
    });
  }
}



void npmstorage::add_new_release(name user, name repo, std::string package_and_version, uint32_t load_order) {
  eosio::check(load_order == LOAD_ORDER_STRICT || load_order == LOAD_ORDER_ANY, "invalid load order!");
  uint64_t packageversion_id = add_package_version(user, package_and_version, false);

  auto packageversions_iterator = tbl_packageversions.find(packageversion_id);
  eosio::check(packageversions_iterator != tbl_packageversions.end(), "invalid packageversion_id after creation!");

  uint64_t current_time = eosio::current_time_point().sec_since_epoch();



  uint64_t new_release_id = tbl_releases.available_primary_key();
  tbl_releases.emplace(user, [&](auto &row) {
    row.id = new_release_id;

    row.repo = repo;
    row.package_name_id = packageversions_iterator->package_name_id;
    row.package_version_id = packageversions_iterator->id;
    row.package_and_version = package_and_version;

    row.file_count = 0;
    row.load_order = load_order;

    row.status = RELEASE_STATUS_DISABLED;
    row.created_at = current_time;
  });


  tbl_packageversions.modify(packageversions_iterator, user, [&](auto &row) {
    row.num_releases = row.num_releases+1;
  });
}

void npmstorage::set_release_status(name user, uint64_t release_id, uint32_t status){
  eosio::check(status == RELEASE_STATUS_DISABLED || status == RELEASE_STATUS_ACTIVE, "invalid release status!");
  auto releases_iterator = assert_user_owns_release(user, release_id);
  eosio::check(releases_iterator != tbl_releases.end(), "release does not exist!");
  tbl_releases.modify(releases_iterator, user, [&](auto &row) {
    row.status = status;
  });
}


void npmstorage::set_release_load_order(name user, uint64_t release_id, uint32_t load_order){
  eosio::check(load_order == LOAD_ORDER_STRICT || load_order == LOAD_ORDER_ANY, "invalid load order!");
  auto releases_iterator = assert_user_owns_release(user, release_id);
  eosio::check(releases_iterator != tbl_releases.end(), "release does not exist!");
  tbl_releases.modify(releases_iterator, user, [&](auto &row) {
    row.load_order = load_order;
  });
}

void npmstorage::add_release_file(name user, uint64_t release_id, checksum256 filehash, std::string alt_sources, std::string externals, uint32_t file_type, uint32_t load_index){
  eosio::check(is_valid_file_type(file_type), "invalid file type!");
  eosio::check(load_index >= 0, "invalid load_index!");
  eosio::check(validate_resource_file_alt_sources(alt_sources), "invalid alt_sources!");
  eosio::check(validate_resource_file_externals(externals), "invalid alt_sources!");
  
  auto releases_iterator = assert_user_owns_release(user, release_id);
  eosio::check(releases_iterator != tbl_releases.end(), "release does not exist!");


  checksum256 rloadindex = get_rloadindex(release_id, load_index);

  auto rloadindex_index = tbl_releasefiles.get_index<"byrloadindex"_n>();
  auto rloadindex_index_iterator = rloadindex_index.find(rloadindex);
  eosio::check(rloadindex_index_iterator == rloadindex_index.end(), "file already already exists with this load_index");

  if(load_index > 0){
    auto prev_file_iterator = rloadindex_index.find(get_rloadindex(release_id, load_index-1));
    eosio::check(prev_file_iterator != rloadindex_index.end(), "the previous load_index has not yet been populated!");
  }
  auto data_hash_index = tbl_resources.get_index<"datahashidx"_n>();
  auto data_hash_iterator = data_hash_index.find(filehash);
  eosio::check(data_hash_iterator != data_hash_index.end(), "resource does not exist");


  uint64_t new_id = tbl_releasefiles.available_primary_key();
  tbl_releasefiles.emplace(user, [&](auto &row) {
    row.id = new_id;
    row.release_id = release_id;
    row.package_version_id = releases_iterator->package_version_id;

    row.repo = releases_iterator->repo;
    row.load_index = load_index;
    row.file_type = file_type;

    row.sha256hash = filehash;
    row.resource_id = data_hash_iterator->rid;

    row.alt_sources = alt_sources;
    row.externals = externals;
    row.rloadindex = rloadindex;
  });
}



/*

ACTION npmstorage::addpkgver(name user, std::string package_and_version){
  // validate caller permissions as the uploader
  require_auth(user);
  add_package_version(user, package_and_version, true);
}

*/

ACTION npmstorage::devclearall(){
  // FOR DEVELOPMENT/TESTING NETWORKS ONLY: delete this action if shipping to the mainnet!
  require_auth(DEBUG_CONTRACT_ADMIN);

  auto packageversions_iterator = tbl_packageversions.begin();
  while (packageversions_iterator != tbl_packageversions.end()) {
    packageversions_iterator = tbl_packageversions.erase(packageversions_iterator);
  }
  
  auto packagenames_iterator = tbl_packagenames.begin();
  while (packagenames_iterator != tbl_packagenames.end()) {
    packagenames_iterator = tbl_packagenames.erase(packagenames_iterator);
  }

  auto stringstore_iterator = tbl_stringstore.begin();
  while (stringstore_iterator != tbl_stringstore.end()) {
    stringstore_iterator = tbl_stringstore.erase(stringstore_iterator);
  }

  auto releases_iterator = tbl_releases.begin();
  while (releases_iterator != tbl_releases.end()) {
    releases_iterator = tbl_releases.erase(releases_iterator);
  }

  auto releasefiles_iterator = tbl_releasefiles.begin();
  while (releasefiles_iterator != tbl_releasefiles.end()) {
    releasefiles_iterator = tbl_releasefiles.erase(releasefiles_iterator);
  }

  auto repos_iterator = tbl_repos.begin();
  while (repos_iterator != tbl_repos.end()) {
    repos_iterator = tbl_repos.erase(repos_iterator);
  }
  
  auto resources_iterator = tbl_resources.begin();
  while (resources_iterator != tbl_resources.end()) {
    resources_iterator = tbl_resources.erase(resources_iterator);
  }
  
}


ACTION npmstorage::upsertrepo(name user, name repo, std::string title, std::string description, std::string url, std::string icon){
  require_auth(user);
  upsert_repo_content(user, repo, title, description, url, icon);
}

ACTION npmstorage::addrelease(name user, name repo, std::string package_and_version, uint32_t load_order){
  require_auth(user);
  add_new_release(user, repo, package_and_version, load_order);
}

ACTION npmstorage::delrelease(name user, uint64_t release_id){
  require_auth(user);
  eosio::check(false, "this function is not currently supported");

}

ACTION npmstorage::setreleaseon(name user, uint64_t release_id, uint32_t status){
  require_auth(user);
  set_release_status(user, release_id, status);
}
ACTION npmstorage::setloadorder(name user, uint64_t release_id, uint32_t load_order){
  require_auth(user);
  set_release_load_order(user, release_id, load_order);
}

ACTION npmstorage::addrelfile(name user, uint64_t release_id, checksum256 filehash, std::string alt_sources, std::string externals, uint32_t file_type, uint32_t load_index){
  require_auth(user);
  add_release_file(user, release_id, filehash, alt_sources, externals, file_type, load_index);
}

ACTION npmstorage::swaploadind(name user, uint64_t release_id, uint64_t releasefile_id_a, uint64_t releasefile_id_b){

}



ACTION npmstorage::add(name uploader, checksum256 sha256hash, std::string data) {
  // validate caller permissions as the uploader
  require_auth(uploader);

  // ensure that the sha256hash passed by the user is the real sha256 hash of data
  assert_sha256(data.c_str(), data.length(), sha256hash);

  auto data_hash_index = tbl_resources.get_index<"datahashidx"_n>();
  auto data_hash_iterator = data_hash_index.find(sha256hash);
  eosio::check(data_hash_iterator == data_hash_index.end(), "resource already exists");

  uint64_t new_rid = tbl_resources.available_primary_key();
  tbl_resources.emplace(uploader, [&](auto &row) {
    row.rid = new_rid;
    row.uploader = uploader;
    row.sha256hash = sha256hash;
    row.data = data;
  });
}
