#include "external/run_file.hpp"
#include <iostream>
#include <string>

using namespace external;
int main(int argc, char** argv) {
    try {
        const std::filesystem::path path=argc>1 ? argv[1] : "week3_demo.runs";
        IOStats write_stats;
        RunFileWriter writer(path,5,3,write_stats);
        // Small teaching inputs; no full external file is loaded or sorted.
        writer.begin_run();
        for (Value v : {-5,1,1,8,20}) writer.put(v);
        writer.end_run();
        writer.begin_run(); writer.end_run();
        writer.begin_run();
        for (Value v : {0,3,9}) writer.put(v);
        writer.end_run(); writer.finish();
        IOStats read_stats;
        RunFileReader reader(path,read_stats);
        reader.display(std::cout,3);
        std::cout<<"Scan Run 0 [1,4):";
        reader.scan(0,1,3,3,[](Value v){std::cout<<' '<<v;});
        const auto found=reader.search(0,1,3);
        std::cout<<"\nSearch Run 0 key=1: "<<(found?std::to_string(*found):"not found")
                 <<"\nSearch Run 0 key=7: "<<(reader.search(0,7,3)?"found":"not found")
                 <<"\nData writes="<<write_stats.data_writes<<" bytes="<<write_stats.data_write_bytes
                 <<"\nData reads="<<read_stats.data_reads<<" bytes="<<read_stats.data_read_bytes
                 <<"\nMetadata writes="<<write_stats.metadata_writes<<" reads="<<read_stats.metadata_reads<<'\n';
        return 0;
    } catch (const std::exception& e) { std::cerr<<"Error: "<<e.what()<<'\n'; return 1; }
}
