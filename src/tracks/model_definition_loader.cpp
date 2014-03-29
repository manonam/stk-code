//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2007-2013 Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "tracks/model_definition_loader.hpp"
using namespace irr;

#include "graphics/irr_driver.hpp"
#include "graphics/lod_node.hpp"
#include "graphics/stkinstancedscenenode.hpp"
#include "io/xml_node.hpp"
#include "tracks/track.hpp"

#include <IMeshSceneNode.h>
#include <ISceneManager.h>
#include <IMeshManipulator.h>
#include <algorithm>

ModelDefinitionLoader::ModelDefinitionLoader(Track* track)
{
    m_track = track;
}

// ----------------------------------------------------------------------------

void ModelDefinitionLoader::addModelDefinition(const XMLNode* xml)
{
    float lod_distance = -1.0f;
    xml->get("lod_distance", &lod_distance);

    std::string lodgroup;
    xml->get("lod_group", &lodgroup);

    bool tangent = false;
    xml->get("tangents", &tangent);

    std::string model_name;
    xml->get("model", &model_name);

    m_lod_groups[lodgroup].push_back(ModelDefinition(xml, (int)lod_distance, model_name, tangent));
}

// ----------------------------------------------------------------------------

LODNode* ModelDefinitionLoader::instanciateAsLOD(const XMLNode* node, scene::ISceneNode* parent)
{
    scene::ISceneManager* sm = irr_driver->getSceneManager();

    std::string groupname = "";
    node->get("lod_group", &groupname);

    std::vector< ModelDefinition >& group = m_lod_groups[groupname];

    if (group.size() > 0)
    {
        scene::ISceneNode* actual_parent = (parent == NULL ? sm->getRootSceneNode() : parent);
        LODNode* lod_node = new LODNode(groupname, actual_parent, sm);
        lod_node->updateAbsolutePosition();
        for (unsigned int m=0; m<group.size(); m++)
        {
            // TODO: check whether the mesh contains animations or not?
            scene::IMesh* a_mesh = irr_driver->getMesh(group[m].m_model_file);

            if (!a_mesh)
            {
                Log::warn("LODNodeLoad", "Warning: object model '%s' not found, ignored.\n",
                          group[m].m_model_file.c_str());
                continue;
            }

            if (group[m].m_tangent && a_mesh->getMeshBuffer(0)->getVertexType() != video::EVT_TANGENTS)
            {
                scene::IMeshManipulator* manip = irr_driver->getVideoDriver()->getMeshManipulator();
                scene::IMesh* m2 = manip->createMeshWithTangents(a_mesh);
                // FIXME: do we need to clean up 'a_mesh' ?
                a_mesh = m2;
                irr_driver->setAllMaterialFlags(a_mesh);
            }

            a_mesh->grab();
            //cache.push_back(a_mesh);
            irr_driver->grabAllTextures(a_mesh);
            scene::IMeshSceneNode* scene_node = irr_driver->addMesh(a_mesh);

            m_track->handleAnimatedTextures( scene_node, *group[m].m_xml );

            lod_node->add( group[m].m_distance, scene_node, true );
        }

#ifdef DEBUG
        std::string debug_name = groupname+" (LOD track-object)";
        lod_node->setName(debug_name.c_str());
#endif
        return lod_node;
    }
    else
    {
        Log::warn("ModelDefinitionLoader", "LOD group '%s' is empty", groupname.c_str());
        return NULL;
    }
}

// ----------------------------------------------------------------------------

STKInstancedSceneNode* ModelDefinitionLoader::instanciate(const irr::core::vector3df& position,
                                const irr::core::vector3df& rotation,
                                const std::string& name)
{
    if (m_instancing_nodes.find(name) == m_instancing_nodes.end())
    {
        if (m_lod_groups.find(name) == m_lod_groups.end())
        {
            Log::warn("Instancing", "Cannot find instancing model <%s>", name.c_str());
            return NULL;
        }

        scene::IMesh* mesh = irr_driver->getMesh(m_lod_groups[name][0].m_model_file);
        m_instancing_nodes[name] = new STKInstancedSceneNode(mesh,
            irr_driver->getSceneManager()->getRootSceneNode(), irr_driver->getSceneManager(), -1);
        m_track->addNode(m_instancing_nodes[name]);
    }

    m_instancing_nodes[name]->addInstance(position, rotation);
    return m_instancing_nodes[name];
}

// ----------------------------------------------------------------------------

void ModelDefinitionLoader::clear()
{
    m_lod_groups.clear();
}
