/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/
#include "ignition/gazebo/Entity.hh"

using namespace ignition::gazebo;

/////////////////////////////////////////////////
Entity::Entity(const EntityId _id)
  : id(_id)
{
}

/////////////////////////////////////////////////
Entity::Entity(Entity &&_entity) noexcept
  : id(_entity.id)
{
  _entity.id = kNullEntity;
}

/////////////////////////////////////////////////
bool Entity::operator==(const Entity &_entity) const
{
  return this->id == _entity.id;
}

/////////////////////////////////////////////////
Entity &Entity::operator=(Entity &&_entity)
{
  this->id = _entity.id;
  _entity.id = kNullEntity;
  return *this;
}

/////////////////////////////////////////////////
EntityId Entity::Id() const
{
  return this->id;
}
