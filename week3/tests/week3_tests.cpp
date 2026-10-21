#include "external/run_file.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace external;
namespace {
int assertions=0, groups=0;
void check(bool ok, const char* message) {
    ++assertions;
    if (!ok) throw std::runtime_error(message);
}
template<class F> void rejects(F f) {
    bool thrown=false;
    try { f(); } catch (const std::exception&) { thrown=true; }
    check(thrown,"expected exception");
}
void passed(const char* name) { ++groups; std::cout<<"PASS "<<name<<'\n'; }
// Own a newly created directory only; never remove an existing user directory.
struct Sandbox {
    std::filesystem::path path;
    Sandbox() {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        path=std::filesystem::temp_directory_path()/("week3_tests_"+std::to_string(stamp));
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("test directory collision");
    }
    ~Sandbox() { std::error_code ec; std::filesystem::remove_all(path,ec); }
};
void patch(const std::filesystem::path& p, std::uint64_t offset, std::uint64_t value, unsigned bytes=8) {
    std::fstream f(p,std::ios::binary|std::ios::in|std::ios::out);
    char b[8]{};
    // Independent little-endian serializer for fault injection.
    for (unsigned i=0;i<bytes;++i) b[i]=static_cast<char>((value>>(8*i))&255);
    f.seekp(static_cast<std::streamoff>(offset)); f.write(b,bytes);
    if (!f) throw std::runtime_error("test patch failed");
}
}
int main() {
 try {
    Sandbox sandbox;
    const auto p=sandbox.path/"example.runs";
    IOStats ws;
    {
        RunFileWriter w(p,5,3,ws);
        w.begin_run(); for (Value v:{-5,1,1,8,20}) w.put(v); w.end_run();
        w.begin_run(); w.end_run();
        w.begin_run(); for (Value v:{0,3,9}) w.put(v); w.end_run(); w.finish();
        rejects([&]{w.begin_run();}); rejects([&]{w.finish();});
    }
    check(ws.data_writes==3 && ws.data_write_bytes==32,"writer block count");
    check(ws.metadata_writes==5 && ws.metadata_write_bytes==384,"writer metadata count");
    check(std::filesystem::file_size(p)==256,"exact file size");
    // Inspect bytes independently of RunFileReader: catches paired encoder/decoder bugs.
    {
        std::ifstream f(p,std::ios::binary);
        std::vector<unsigned char> raw(256);
        f.read(reinterpret_cast<char*>(raw.data()),256);
        check(raw[0]=='R' && raw[7]==10 && raw[8]==1 && raw[12]==4,"raw header");
        check(raw[16]==5 && raw[24]==3 && raw[32]==64 && raw[40]==224,"raw header positions");
        check(raw[48]==0 && raw[49]==1,"exclusive end little endian");
        check(raw[72]==5 && raw[80]==224 && raw[88]==20,"raw entry 0");
        check(raw[104]==0 && raw[112]==244 && raw[144]==244,"empty run offsets");
        check(raw[224]==251 && raw[225]==255 && raw[226]==255 && raw[227]==255,"negative encoding");
    }
    IOStats rs;
    RunFileReader r(p,rs);
    check(rs.metadata_reads==4 && rs.metadata_read_bytes==160,"open metadata validation");
    check(r.run_count()==3 && r.max_runs()==5 && r.data_start()==224 && r.file_end()==256,"header accessors");
    check(r.directory(2).offset==244 && r.directory(2).count==3,"directory offset");
    passed("format, unequal runs, raw bytes and metadata I/O");

    rs={};
    std::vector<Value> values;
    r.scan(0,0,5,3,[&](Value v){values.push_back(v);});
    check(values==std::vector<Value>({-5,1,1,8,20}),"full scan");
    check(rs.data_reads==2 && rs.data_read_bytes==20,"full scan I/O");
    rs={}; values.clear();
    r.scan(0,1,3,3,[&](Value v){values.push_back(v);});
    check(values==std::vector<Value>({1,1,8}) && rs.data_reads==2,"cross-block scan");
    rs={};
    r.scan(1,0,0,3,[](Value){throw std::runtime_error("empty visitor called");});
    r.scan(0,5,0,3,[](Value){throw std::runtime_error("EOF visitor called");});
    check(rs.data_reads==0,"empty scan has no data I/O");
    rejects([&]{r.scan(0,6,0,3,[](Value){});});
    rejects([&]{r.scan(0,4,2,3,[](Value){});});
    rejects([&]{r.scan(0,1,std::numeric_limits<std::uint64_t>::max(),3,[](Value){});});
    rejects([&]{r.input(3,3);}); rejects([&]{r.input(0,0);});
    std::ostringstream display; r.display(display,3);
    check(display.str().find("Run 0 count=5 offset=224 bytes=20: -5 1 1 8 20")!=std::string::npos,"display");
    passed("Scan ranges, empty runs and display");

    rs={};
    auto in=r.input(0,3);
    check(in.at(0)==-5 && in.at(2)==1 && rs.data_reads==1,"cache hit");
    check(in.at(4)==20 && rs.data_reads==2,"tail block load");
    in.seek(5); Value unchanged=123;
    check(!in.next(unchanged) && !in.next(unchanged) && unchanged==123 && rs.data_reads==2,"stable EOF");
    rejects([&]{in.at(5);}); rejects([&]{in.seek(6);});
    check(r.search(0,1,3)==std::optional<std::uint64_t>(1),"first duplicate");
    check(r.search(0,-5,3)==std::optional<std::uint64_t>(0),"first value");
    check(r.search(0,20,3)==std::optional<std::uint64_t>(4),"last value");
    for (Value key:{-6,7,21}) check(!r.search(0,key,3),"missing search");
    check(!r.search(1,0,3),"empty search");
    rs={}; r.search(0,1,3);
    check(rs.data_reads==1 && rs.data_read_bytes==12,"binary search cache I/O");
    passed("InputBuffer cache, EOF and Search lower_bound");

    {
        IOStats s; std::ostringstream stream(std::ios::binary);
        OutputBuffer out(stream,0,3,s);
        out.flush(); check(s.data_writes==0,"empty flush");
        out.put(1); out.put(2); check(s.data_writes==0,"delayed output");
        out.put(3); check(s.data_writes==1,"full block flush");
        out.put(4); out.flush(); out.flush();
        check(s.data_writes==2 && s.data_write_bytes==16 && stream.str().size()==16,"tail flush once");
        std::istringstream short_stream(std::string(3,'x'));
        InputBuffer short_in(short_stream,0,1,1,s);
        rejects([&]{short_in.at(0);});
        std::ostringstream failed; failed.setstate(std::ios::badbit);
        OutputBuffer bad(failed,0,1,s); rejects([&]{bad.put(1);});
        rejects([&]{OutputBuffer invalid(stream,0,0,s);});
    }
    passed("OutputBuffer flush and failed/short I/O");

    {
        IOStats s; const auto q=sandbox.path/"states.runs";
        RunFileWriter w(q,1,2,s);
        rejects([&]{w.put(1);}); rejects([&]{w.end_run();});
        w.begin_run(); rejects([&]{w.begin_run();}); rejects([&]{w.finish();});
        w.put(std::numeric_limits<Value>::min()); w.put(0);
        rejects([&]{w.put(-1);}); // rejected Put must not change the active run
        w.put(std::numeric_limits<Value>::max()); w.end_run();
        rejects([&]{w.begin_run();}); w.finish();
        RunFileReader reader(q,s);
        check(reader.directory(0).count==3 && reader.validate_sorted(0,1),"rejected Put preserves state");
        check(reader.search(0,std::numeric_limits<Value>::min(),1)==std::optional<std::uint64_t>(0),"int32 min");
        check(reader.search(0,std::numeric_limits<Value>::max(),1)==std::optional<std::uint64_t>(2),"int32 max");
        rejects([&]{RunFileWriter overwrite(q,1,2,s);});
        const auto empty=sandbox.path/"empty.runs";
        RunFileWriter ew(empty,0,1,s); rejects([&]{ew.begin_run();}); ew.finish();
        RunFileReader er(empty,s); check(er.run_count()==0 && er.file_end()==64,"zero-run file");
        const auto unfinished=sandbox.path/"unfinished.runs";
        {RunFileWriter uw(unfinished,1,2,s); uw.begin_run(); uw.put(9);}
        rejects([&]{RunFileReader ur(unfinished,s);});
        rejects([&]{RunFileWriter invalid(sandbox.path/"bad.runs",1048577,1,s);});
        rejects([&]{RunFileReader missing(sandbox.path/"missing",s);});
    }
    passed("writer protocol, order rejection, limits and unfinished files");

    {
        IOStats s;
        const std::vector<std::pair<std::uint64_t,std::uint64_t>> faults={
            {0,0},{8,2},{16,1048577},{24,6},{32,65},{40,223},{48,255},{56,1},
            {64,9},{72,std::numeric_limits<std::uint64_t>::max()},{80,223},{88,16},
            {112,240},{144,248}};
        int index=0;
        for (const auto& f:faults) {
            const auto q=sandbox.path/("corrupt"+std::to_string(index++));
            std::filesystem::copy_file(p,q); patch(q,f.first,f.second);
            rejects([&]{RunFileReader invalid(q,s);});
        }
        for (std::uint64_t size:{0ULL,63ULL,230ULL,255ULL,257ULL}) {
            const auto q=sandbox.path/("size"+std::to_string(size));
            std::filesystem::copy_file(p,q); std::filesystem::resize_file(q,size);
            rejects([&]{RunFileReader invalid(q,s);});
        }
        const auto q=sandbox.path/"unsorted";
        std::filesystem::copy_file(p,q); patch(q,228,100,4);
        RunFileReader bad_order(q,s);
        check(!bad_order.validate_sorted(0,2),"explicit data order validation");
    }
    passed("corrupt header/directory, truncation and unsorted data");

    {
        // Bounded small oracle vectors belong to tests, never the storage implementation.
        std::mt19937 rng(123456);
        for (std::size_t capacity:{1U,2U,3U,16U,100U}) {
            IOStats s; const auto q=sandbox.path/("random"+std::to_string(capacity));
            std::vector<std::vector<Value>> expected(20);
            RunFileWriter w(q,20,capacity,s);
            std::uint64_t blocks=0, bytes=0;
            for (auto& run:expected) {
                run.resize(rng()%70);
                for (auto& v:run) v=static_cast<Value>(rng()%101)-50;
                std::sort(run.begin(),run.end());
                w.begin_run(); for (auto v:run) w.put(v); w.end_run();
                blocks+=(run.size()+capacity-1)/capacity; bytes+=run.size()*4;
            }
            w.finish(); check(s.data_writes==blocks && s.data_write_bytes==bytes,"random write I/O oracle");
            RunFileReader reader(q,s); s={};
            for (std::size_t i=0;i<expected.size();++i) {
                std::vector<Value> actual;
                reader.scan(i,0,expected[i].size(),capacity,[&](Value v){actual.push_back(v);});
                check(actual==expected[i],"random scan oracle");
            }
            check(s.data_reads==blocks && s.data_read_bytes==bytes,"random read I/O oracle");
            for (std::size_t i=0;i<expected.size();++i) for (Value key=-55;key<=55;++key) {
                const auto pos=std::lower_bound(expected[i].begin(),expected[i].end(),key);
                std::optional<std::uint64_t> answer;
                if (pos!=expected[i].end() && *pos==key) answer=static_cast<std::uint64_t>(pos-expected[i].begin());
                check(reader.search(i,key,capacity)==answer,"random search oracle");
            }
        }
    }
    passed("deterministic randomized oracle across 5 buffer capacities");

    {
        IOStats s; const auto q=sandbox.path/"large.runs";
        constexpr std::uint64_t n=1000003, capacity=127;
        RunFileWriter w(q,1,capacity,s); w.begin_run();
        for (std::uint64_t i=0;i<n;++i) w.put(static_cast<Value>(i));
        w.end_run(); w.finish();
        check(s.data_writes==(n+capacity-1)/capacity && s.data_write_bytes==n*4,"large write I/O");
        RunFileReader reader(q,s); s={}; std::uint64_t count=0, sum=0;
        reader.scan(0,0,n,capacity,[&](Value v){check(v==static_cast<Value>(count),"large streaming order"); ++count; sum+=v;});
        check(count==n && sum==n*(n-1)/2,"large count/checksum");
        check(s.data_reads==(n+capacity-1)/capacity && s.data_read_bytes==n*4,"large read I/O");
        std::cout<<"Large streaming case: "<<n<<" values, buffer="<<capacity
                 <<", reads="<<s.data_reads<<", bytes="<<s.data_read_bytes<<'\n';
    }
    passed("million-record streaming data with bounded buffers");
    std::cout<<"ALL PASSED: "<<groups<<" groups, "<<assertions<<" assertions\n";
    return 0;
 } catch (const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
