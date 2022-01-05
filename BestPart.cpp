#include "Bloom.h"
#include "ExponentialBloom.h"
#include "utils.h"
#include "bestpart.h"
#include "best.h"
#include <iostream>
#include <vector>
#include <math.h>
#include <cmath> 
#include <string>
#include <chrono>
#include <ctime>




using namespace std;



uint64_t str2num(const string& str) {
	uint64_t res(0);
	for (uint64_t i(0); i < str.size(); i++) {
		res <<= 2;
		res += (str[i] / 2) % 4;
	}
	return res;
}



uint64_t revhash(uint64_t x) {
	// return hash64shift(x);
	x = ((x >> 32) ^ x) * 0xD6E8FEB86659FD93;
	x = ((x >> 32) ^ x) * 0xD6E8FEB86659FD93;
	x = ((x >> 32) ^ x);
	return x;
}



uint64_t unrevhash(uint64_t x) {
	return hash64shift(x);
	x = ((x >> 32) ^ x) * 0xCFEE444D8B59A89B;
	x = ((x >> 32) ^ x) * 0xCFEE444D8B59A89B;
	x = ((x >> 32) ^ x);
	return x;
}



template <class T>
void BestPart<T>::insert_keys(const vector<uint64_t>& key,uint minimizer,uint level,Bloom* unique_filter){
    if(filter){
        for(uint i=0;i<key.size(); ++i){
            if(unique_filter->check_key(key[i])){
                buckets[minimizer]->insert_key(key[i],level);
            }else{
                unique_filter->insert_key(key[i]);
            }
        }
    }else{
        for(uint i=0;i<key.size(); ++i){
            buckets[minimizer]->insert_key(key[i],level);
        }
    }
}



template <class T>
vector<T> BestPart<T>::query_keys(const vector<uint64_t>& key,uint minimizer){
    vector<T> result;
    vector<T> colors;
    for(uint i=0;i<key.size(); ++i){
        // cout<<"go query key"<<key[i]<<endl;
        colors=buckets[minimizer]->query_key(key[i]);
        // cout<<"OK query key"<<key[i]<<endl;
        result.insert( result.end(), colors.begin(), colors.end() );
        colors.clear();
    }
    return result;
}



//TODO CAN BE IMPROVED?
template <class T>
uint64_t BestPart<T>::rcb(uint64_t min, uint64_t n)const {
  uint64_t res(0);
  uint64_t offset(1);
  offset<<=(2*n-2);
  for(uint i(0); i<n;++i){
    res+=(3-(min%4))*offset;
    min>>=2;
    offset>>=2;
  }
  return res;
}



template <class T>
uint64_t BestPart<T>::canonize(uint64_t x, uint64_t n) {
	return min(x, rcb(x, n));
}



template <class T>
uint64_t BestPart<T>::regular_minimizer_pos(uint64_t seq, uint64_t& position) {
	uint64_t mini, mmer;
	mmer = seq % large_minimizer_number;
	mini = mmer        = canonize(mmer, large_minimizer_size);
	uint64_t hash_mini = (unrevhash(mmer));
	position           = 0;
	for (uint64_t i(1); i <= K - large_minimizer_size; i++) {
		seq >>= 2;
		mmer          = seq % large_minimizer_number;
		mmer          = canonize(mmer, large_minimizer_size);
		uint64_t hash = (unrevhash(mmer));
		if (hash_mini > hash) {
			position  = K - large_minimizer_size - i;
			mini      = mmer;
			hash_mini = hash;
		}
	}
	return mini;
}



template <class T>
void BestPart<T>::updateK(uint64_t& min, char nuc)const {
  min<<=2;
  min+=nuc2int(nuc);
  min%=offsetUpdatekmer;
}


template <class T>
void BestPart<T>::updateM(uint64_t& min, char nuc)const {
  min<<=2;
  min+=nuc2int(nuc);
  min%=offsetUpdateminimizer;
}



template <class T>
void BestPart<T>::updateRCK(uint64_t& min, char nuc)const {
  min>>=2;
  min+=(nuc2intrc(nuc)<<(2*K-2));
}



template <class T>
void BestPart<T>::updateRCM(uint64_t& min, char nuc)const {
  min>>=2;
  min+=(nuc2intrc(nuc)<<(2*large_minimizer_size-2));
}



template <class T>
void BestPart<T>::change_level(){
    for(uint32_t i=0;i<bucket_number;++i){
        buckets[i]->change_level();
    }
}



template <class T>
vector<pair<vector<uint64_t>,uint64_t> > BestPart<T>::get_super_kmers(const string& ref) {
    vector<pair<vector<uint64_t>,uint64_t> > result;
    uint64_t old_minimizer, minimizer;
    vector<uint64_t> superkmer;
    old_minimizer = minimizer = large_minimizer_number;
    uint64_t last_position(0);
    // FOREACH KMER
    uint64_t seq(str2num(ref.substr(0, K)));
    uint64_t rcseq=rcb(seq,K);
    uint64_t canon=min(seq,rcseq);
    uint64_t position_min;
    uint64_t min_seq = (str2num(ref.substr(K - large_minimizer_size, large_minimizer_size)));
    uint64_t min_rcseq(rcb(min_seq, large_minimizer_size));
    uint64_t min_canon(min(min_seq, min_rcseq));
    minimizer         = regular_minimizer_pos(seq, position_min);
    old_minimizer     = minimizer;
    uint64_t hash_min = unrevhash(minimizer);
    uint64_t i(0);
    for (; i + K < ref.size(); ++i) {
        updateK(seq, ref[i + K]);
        updateRCK(rcseq, ref[i + K]);
        canon=min(seq,rcseq);
        updateM(min_seq, ref[i + K]);
        updateRCM(min_rcseq, ref[i + K]);
        min_canon      = (min(min_seq, min_rcseq));
        uint64_t new_h = unrevhash(min_canon);
        // THE NEW mmer is a MINIMIZER
        if (new_h < hash_min) {
            minimizer    = (min_canon);
            hash_min     = new_h;
            position_min = i + K - large_minimizer_size + 1;
        } else {
            // the previous minimizer is outdated
            if (i >= position_min) {
                minimizer = regular_minimizer_pos(seq, position_min);
                hash_min  = unrevhash(minimizer);
                position_min += (i + 1);
            } else {
            }
        }
        // COMPUTE KMER MINIMIZER
        if (revhash(old_minimizer) % bucket_number != revhash(minimizer) % bucket_number) {
            old_minimizer = (revhash(old_minimizer) % bucket_number);
            result.push_back({superkmer,old_minimizer});
            superkmer.clear();
            last_position = i + 1;
            old_minimizer = minimizer;
        }
        superkmer.push_back(canon);
    }
    if (ref.size() - last_position > K - 1) {
        old_minimizer = (revhash(old_minimizer) % bucket_number);
        result.push_back({superkmer,old_minimizer});
        superkmer.clear();
    }
    return result;
}



template <class T>
void BestPart<T>::insert_sequence(const string& reference,uint level, Bloom* unique_filter){
    auto V(get_super_kmers(reference));
    // #pragma omp parallel for
    for(uint32_t i = 0; i < V.size();++i){
        insert_keys(V[i].first, V[i].second,level,unique_filter);
    }
}



template <class T>
void BestPart<T>::query_file(const string& filename, const string& output){
    cout<<"Query file "<<filename<<" in "<<output<<endl;
    auto  start = chrono::system_clock::now();
    zstr::ofstream out(output,ios::binary);
    char type=get_data_type(filename);
    zstr::ifstream in(filename);
    #pragma omp parallel
    {
        string ref,header;
        vector<uint32_t> colors_count;
        while(not in.eof()) {
            #pragma omp critical (inputfile)
            {
             Biogetline(&in,ref,type,K,header);
            }
            if(ref.size()>K) {
                // cout<<"query_sequence part go"<<endl;
                colors_count=query_sequence(ref);
                //  cout<<"query_sequence  part end"<<endl;
                #pragma omp critical (output_file)
                {   
                    out<<header<<"\n";
                    for(uint i=0;i<colors_count.size();i++){
                        out<<colors_count[i]<<' ';
                    }
                    out<<"\n";
                }
            }
        }
    }
    out.close();
    auto end = std::chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = end - start;
    cout <<  "Query time: " << elapsed_seconds.count() << "s\n";
}








template <class T>
vector<uint32_t> BestPart<T>::query_sequence(const string& reference){
    // cout<<"Query sequence go"<<endl;
    vector<uint32_t> result(leaf_number,0);
    auto V(get_super_kmers(reference));
    #pragma omp parallel for
    for(uint32_t i = 0; i < V.size();++i){
        // cout<<"query super kmer go"<<endl;
        omp_set_lock(&mutex_array[V[i].second]);
        vector<T> colors=query_keys(V[i].first, V[i].second);
        buckets[V[i].second]->free_ram();
        omp_unset_lock(&mutex_array[V[i].second]);
        // cout<<"query super kmer OK"<<endl;
        for(uint32_t j = 0; j <colors.size();++j){
            #pragma omp atomic
                result[colors[j]]++;
        }
    }
    
    return result;
}



template <class T>
void  BestPart<T>::insert_file(const string filename, uint level){
    char type=get_data_type(filename);
    // cout<<"insert_file"<<filename<<endl;
    zstr::ifstream in(filename);
    Bloom* unique_filter;
    if(filter){
        unique_filter=new Bloom(leaf_filters_size,number_hash_function,"osef");
    }
    // #pragma omp parallel
    {
        string ref;
        while(not in.eof()) {
            // #pragma omp critical (inputfile)
            {
                Biogetline(&in,ref,type,K);
            }
            if(ref.size()>K) {
                insert_sequence(ref,level,unique_filter);
            }
        }
    }
    if(filter){
        delete unique_filter;
    }

    // #pragma omp parallel for
    for(uint i=0;i<buckets.size();++i){
        buckets[i]->optimize(level);
        if(not hot){
            // cout<<"NOT HOT"<<endl;
            buckets[i]->dump(level);
        }
    }
}



template <class T>
void BestPart<T>::insert_file_of_file(const string filename){
    cout<<"Insert file of file "<<filename<<endl;
    auto  start = chrono::system_clock::now();
    uint i = 0;
    zstr::ifstream in(filename);
    #pragma omp parallel
    {
        string ref;
        uint level;
        bool go;
        while(not in.eof()) {
            #pragma omp critical (inputfile)
            {
                getline(in,ref);
                if(exists_test(ref)){
                    level=i;
                    i++;
                    add_leaf();
                    go=true;
                }else{
                    go=false;
                }
            }
            if(go){
                insert_file(ref,level);
                if(level%10==0){
                    cout<<level<<" files"<<endl;
                    // cout<<intToString(nb_insertions())<<endl;
                }
            }
        }
    }
    auto  middle = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = middle - start;
    cout <<  "Bloom construction time: " << elapsed_seconds.count() << "s\n";
    cout<<intToString(getMemorySelfMaxUsed()/1000)<<" MB total"<<endl;
    cout<<intToString(getMemorySelfMaxUsed()/(1000*i))<<" MB per file"<<endl;
    index();
    auto  end = chrono::system_clock::now();
    elapsed_seconds = end - middle;
    cout <<  "Exponential Bloom construction time: " << elapsed_seconds.count() << "s\n";
    elapsed_seconds = end - start;
    cout <<  "Total Index time: " << elapsed_seconds.count() << "s\n";
    cout<<intToString(getMemorySelfMaxUsed()/1000)<<" MB total"<<endl;
    cout<<intToString(getMemorySelfMaxUsed()/(1000*i))<<" MB per file"<<endl;
}



template <class T>
void BestPart<T>::get_stats()const{
    uint64_t total = 0;
    for(uint i=0; i<bucket_number;++i){
        total += buckets[i]->nb_insertions;
        cout<<intToString(buckets[i]->nb_insertions)<<' ';
    }
    cout<<endl;
    cout<<intToString(total)<<endl;
}



template <class T>
void BestPart<T>::serialize(const string& filename)const{
    cout<<"Dump the index in "<<filename<<endl;
    filebuf fb;
    fb.open(filename, ios::out | ios::binary | ios::trunc);
	zstr::ostream out(&fb);
    // VARIOUS INTEGERS
	out.write(reinterpret_cast<const char*>(&K), sizeof(K));
    out.write(reinterpret_cast<const char*>(&leaf_filters_size), sizeof(leaf_filters_size));
    out.write(reinterpret_cast<const char*>(&trunk_size), sizeof(trunk_size));
    out.write(reinterpret_cast<const char*>(&number_hash_function), sizeof(number_hash_function));
    out.write(reinterpret_cast<const char*>(&small_minimizer_size), sizeof(small_minimizer_size));
    out.write(reinterpret_cast<const char*>(&large_minimizer_size), sizeof(large_minimizer_size));
    //buckets
    for(size_t i = 0; i <buckets.size(); ++i){
        buckets[i]->serialize(&out);
    }
    out<<flush;
    fb.close();
}



template <class T>
void BestPart<T>::load(const string& existing_index){
    cout<<"Loading "<<existing_index<<"..."<<endl;
    zstr::ifstream in(existing_index);
     // VARIOUS INTEGERS
    in.read(reinterpret_cast< char*>(&K), sizeof(K));
    in.read(reinterpret_cast< char*>(&leaf_filters_size), sizeof(leaf_filters_size));
    in.read(reinterpret_cast< char*>(&trunk_size), sizeof(trunk_size));
    in.read(reinterpret_cast< char*>(&number_hash_function), sizeof(number_hash_function));
    in.read(reinterpret_cast< char*>(&small_minimizer_size), sizeof(small_minimizer_size));
    in.read(reinterpret_cast< char*>(&large_minimizer_size), sizeof(large_minimizer_size));
    bucket_number=1<<(2*small_minimizer_size);
    large_minimizer_number=1<<(2*large_minimizer_size);
    mutex_array=new omp_lock_t[bucket_number];
    offsetUpdatekmer=1;
    offsetUpdatekmer<<=2*K;
    offsetUpdateminimizer=1;
    offsetUpdateminimizer<<=(2*large_minimizer_size);
    for(uint32_t i=0;i<bucket_number;++i){
        omp_init_lock(&mutex_array[i]);
    }

    buckets.resize(bucket_number,NULL);
    //buckets
    for(size_t i = 0; i <buckets.size(); ++i){
        buckets[i]=new Best<T>(trunk_size/bucket_number,leaf_filters_size/bucket_number,number_hash_function,K,"Leon"+to_string(i));
        buckets[i]->load(&in);
    }
}



template <class T>
void BestPart<T>::index(){
    #pragma omp parallel for
    for(uint i=0;i<buckets.size();++i){
        buckets[i]->construct_trunk();
    }
    leaf_number=buckets[0]->leaf_filters.size();
}



template <class T>
void BestPart<T>::double_index(){
    cout<<"Construct double trunk for faster queries"<<endl;
    #pragma omp parallel for
    for(uint i=0;i<buckets.size();++i){
        buckets[i]->construct_reverse_trunk();
    }
}



template <class T>
void BestPart<T>::add_leaf(){
    #pragma omp parallel for
    for(uint i=0;i<buckets.size();++i){
        buckets[i]->add_leaf();
    }
}



template <class T>
uint64_t BestPart<T>::nb_insertions  ()const{
    uint64_t result(0);
    for(uint i=0;i<buckets.size();++i){
        result+=buckets[i]->nb_insertions;
    }
    return result;
}


