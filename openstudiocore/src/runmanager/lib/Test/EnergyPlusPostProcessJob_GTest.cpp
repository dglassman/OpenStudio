/**********************************************************************
*  Copyright (c) 2008-2014, Alliance for Sustainable Energy.  
*  All rights reserved.
*  
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*  
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*  
*  You should have received a copy of the GNU Lesser General Public
*  Likey = cense along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**********************************************************************/

#include <gtest/gtest.h>
#include "RunManagerTestFixture.hpp"
#include <runmanager/Test/ToolBin.hxx>
#include <resources.hxx>

#include <runmanager/lib/JobFactory.hpp>
#include <runmanager/lib/EnergyPlusPostProcessJob.hpp>
#include <runmanager/lib/RunManager.hpp>
#include <runmanager/lib/Workflow.hpp>

#include <model/Model.hpp>

#include <utilities/idf/IdfFile.hpp>
#include <utilities/idf/IdfObject.hpp>
#include <utilities/data/EndUses.hpp>
#include <utilities/data/Attribute.hpp>
#include <utilities/sql/SqlFile.hpp>

#include <boost/filesystem/path.hpp>

#include <QDir>

using openstudio::Attribute;
using openstudio::IdfFile;
using openstudio::IdfObject;
using openstudio::IddObjectType;
using openstudio::SqlFile;

TEST_F(RunManagerTestFixture, EnergyPlusPostProcessJob)
{
  openstudio::path outdir = openstudio::toPath(QDir::tempPath()) / openstudio::toPath("EnergyPlusPostProcessJobRunTest");
  boost::filesystem::create_directories(outdir);
  openstudio::path db = outdir / openstudio::toPath("EnergyPlusPostProcessJobRunDB");
  openstudio::runmanager::RunManager kit(db, true);

  openstudio::path originalfile = outdir / openstudio::toPath("in.osm");
  openstudio::path infile = outdir / openstudio::toPath("in_short_run.osm");
  openstudio::path weatherdir = resourcesPath() / openstudio::toPath("runmanager") / openstudio::toPath("USA_CO_Golden-NREL.724666_TMY3.epw");

  openstudio::model::Model m = openstudio::model::exampleModel();
  m.save(originalfile, true);


  boost::optional<IdfFile> idfFile = IdfFile::load(originalfile);
  ASSERT_TRUE(idfFile);
  BOOST_FOREACH(IdfObject idfObject, idfFile->getObjectsByType(IddObjectType("RunPeriod"))){
    idfObject.setInt(1, 1);
    idfObject.setInt(2, 1);
    idfObject.setInt(3, 1);
    idfObject.setInt(4, 2);
  }
  idfFile->save(infile, true);
  
  openstudio::runmanager::Workflow workflow("modeltoidf->energyplus->energypluspostprocess");

  workflow.setInputFiles(infile, weatherdir);
  
  // Build list of tools
  openstudio::runmanager::Tools tools 
    = openstudio::runmanager::ConfigOptions::makeTools(
          energyPlusExePath().parent_path(), 
          openstudio::path(), 
          openstudio::path(), 
          openstudio::path(),
          openstudio::path());
  workflow.add(tools);

  openstudio::runmanager::Job job = workflow.create(outdir);

  kit.enqueue(job, true);

  kit.waitForFinished();

  openstudio::runmanager::FileInfo file;
 
  ASSERT_NO_THROW(file = job.treeOutputFiles().getLastByFilename("report.xml"));

  boost::optional<Attribute> report = Attribute::loadFromXml(file.fullPath);
  ASSERT_TRUE(report);

  boost::optional<Attribute> attribute = report->findChildByName("Net Site Energy");
  ASSERT_TRUE(attribute);

  attribute = report->findChildByName("EndUses.Electricity.Cooling.General");
  ASSERT_TRUE(attribute);

  file = job.treeOutputFiles().getLastByFilename("eplusout.sql");

  SqlFile sqlFile(file.fullPath);
  ASSERT_TRUE(sqlFile.connectionOpen());
}

TEST_F(RunManagerTestFixture, EnergyPlusPostProcessJobRerun)
{
  openstudio::path outdir = openstudio::toPath(QDir::tempPath()) / openstudio::toPath("EnergyPlusPostProcessJobRerunTest");
  boost::filesystem::create_directories(outdir);
  openstudio::path db = outdir / openstudio::toPath("EnergyPlusPostProcessJobRerunDB");
  openstudio::runmanager::RunManager kit(db, true);

  openstudio::path originalfile = outdir / openstudio::toPath("in.osm");
  openstudio::path infile = outdir / openstudio::toPath("in_short_run.osm");
  openstudio::path weatherdir = resourcesPath() / openstudio::toPath("runmanager") / openstudio::toPath("USA_CO_Golden-NREL.724666_TMY3.epw");

  openstudio::model::Model m = openstudio::model::exampleModel();
  m.save(originalfile, true);


  boost::optional<IdfFile> idfFile = IdfFile::load(originalfile);
  ASSERT_TRUE(idfFile);
  BOOST_FOREACH(IdfObject idfObject, idfFile->getObjectsByType(IddObjectType("RunPeriod"))){
    idfObject.setInt(1, 1);
    idfObject.setInt(2, 1);
    idfObject.setInt(3, 1);
    idfObject.setInt(4, 2);
  }
  idfFile->save(infile, true);
  
  openstudio::runmanager::Workflow workflow("modeltoidf->energyplus->energypluspostprocess");

  workflow.setInputFiles(infile, weatherdir);
  
  // Build list of tools
  openstudio::runmanager::Tools tools 
    = openstudio::runmanager::ConfigOptions::makeTools(
          energyPlusExePath().parent_path(), 
          openstudio::path(), 
          openstudio::path(), 
          openstudio::path(),
          openstudio::path());
  workflow.add(tools);

  openstudio::runmanager::Job job = workflow.create(outdir);

  kit.enqueue(job, true);

  kit.waitForFinished();

  openstudio::runmanager::FileInfo file;
 
  ASSERT_NO_THROW(file = job.treeOutputFiles().getLastByFilename("report.xml"));

  openstudio::DateTime firsttime = file.lastModified;

  // force rerun of job
  kit.enqueue(job, true);

  kit.waitForFinished();

  ASSERT_NO_THROW(file = job.treeOutputFiles().getLastByFilename("report.xml"));

  openstudio::DateTime secondtime = file.lastModified;

  //Ensure that the report.xml created from the second run has a later lastModified time than the first,
  // confirming that the job tree actually ran a second time.
  EXPECT_GT(secondtime, firsttime);
}

