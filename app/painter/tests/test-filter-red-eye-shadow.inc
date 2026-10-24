/* SPDX-License-Identifier: GPL-3.0-or-later */
void red_eye_raw_and_shadow ()
{
  std::atomic<bool> cancel {false};std::uint64_t comparisons=0;
  for(unsigned red=0;red<256;red++)
    {
      Bytes input(256*256*4);
      for(unsigned green=0;green<256;green++)for(unsigned blue=0;blue<256;blue++)
        {auto p=(green*256+blue)*4;input[p]=red;input[p+1]=green;input[p+2]=blue;input[p+3]=(red+green+blue)&255;}
      for(auto threshold:{-1073741646,-1,0,50,100,200,1073741873})
        {
          Memory source(input),output(input.size());
          require(filter_red_eye_raw_raster(source,output,256,256,threshold,cancel),"Red Eye raw failed");
          require(output.data==expected_shadow(input,256,256,full_region(256,256),threshold,PointOperation::red_eye),"Red Eye rational oracle mismatch");
          comparisons+=256*256;
        }
    }
  unsigned geometry=0;
  for(auto shape:{std::make_pair(1u,1u),std::make_pair(1u,17u),std::make_pair(19u,1u),std::make_pair(67u,66u)})
    {
      auto input=fixture(shape.first,shape.second);auto full=full_region(shape.first,shape.second);
      auto crop=FilterSelectionRegion{true,true,0,0,std::int32_t((shape.first+1)/2),std::int32_t((shape.second+1)/2)};
      auto outside=FilterSelectionRegion{true,false,0,0,0,0};
      for(auto region:{full,crop,outside})for(auto threshold:{-1073741646,0,50,100,1073741873})
        {verify_paths(input,shape.first,shape.second,region,threshold,true,PointOperation::red_eye);geometry++;}
    }
  const auto input=fixture(8,8);Memory source(input),output(input.size());
  for(auto threshold:{INT32_MIN,-1073741647,1073741874,INT32_MAX})
    {
      Bytes published{17,29,43};const auto saved=published;auto untouched=output.data;
      require(!filter_red_eye_threshold_valid(threshold),"Unsafe threshold admitted");
      rejects<std::invalid_argument>([&]{filter_red_eye_raw_raster(source,output,8,8,threshold,cancel);},"Unsafe raw threshold accepted");
      rejects<std::invalid_argument>([&]{filter_point_shadow_vector(input,8,8,full_region(8,8),PointOperation::red_eye,threshold,cancel,published);},"Unsafe shadow threshold accepted");
      require(published==saved&&output.data==untouched,"Unsafe threshold changed output");
    }
  cancellation_errors_and_progress(PointOperation::red_eye,50);
  /* Every raw read/write/flush interruption preserves caller publication. */
  const auto large=fixture(8205,3);const auto chunks=(large.size()+32767)/32768;unsigned faults=0;
  for(auto op:{Operation::read,Operation::write,Operation::flush})for(bool fail:{false,true})
    for(std::size_t at=1;at<=(op==Operation::flush?1:chunks);at++)
      {
        std::atomic<bool> stop{false};auto progress=std::make_shared<FilterProgress>();RawAudit audit(stop,progress);audit.trigger=op;audit.at=at;audit.fail=fail;
        Bytes published{17,29,43};const auto saved=published;
        auto run=[&]{return filter_native_vector(large,stop,published,[&](FilterRaster& in,FilterRaster& out,std::atomic<bool>& c,const FilterRasterFactory&){RawObserved source(in,audit,true),dest(out,audit,false);return filter_red_eye_raw_raster(source,dest,8205,3,50,c,progress);});};
        if(fail)rejects<StorageFailure>(run,"Red Eye raw storage failure ignored");else require(!run(),"Red Eye raw cancellation ignored");
        require(published==saved,"Red Eye raw partial publication");faults++;
      }
  require(comparisons==117440512&&geometry==60,"Red Eye coverage count");
  std::cout<<"Red Eye:117440512 rational raw comparisons;60 vector/spill ROI scenes;4 unsafe thresholds;"<<faults<<" raw I/O fault controls plus shared shadow cancellation/progress matrix passed\n";
}
