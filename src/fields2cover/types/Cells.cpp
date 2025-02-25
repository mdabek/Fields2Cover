//=============================================================================
//    Copyright (C) 2021-2022 Wageningen University - All Rights Reserved
//                     Author: Gonzalo Mier
//                        BSD-3 License
//=============================================================================

#include "fields2cover/types/Cells.h"

namespace f2c::types {

Cells::Cells() {
  data = std::shared_ptr<OGRMultiPolygon>(
    downCast<OGRMultiPolygon*>(
      OGRGeometryFactory::createGeometry(wkbMultiPolygon)),
    [](OGRMultiPolygon* f) {OGRGeometryFactory::destroyGeometry(f);});
}

Cells::Cells(const OGRGeometry* geom) {
  if (wkbFlatten(geom->getGeometryType()) == OGRwkbGeometryType::wkbPolygon) {
    this->data = std::shared_ptr<OGRMultiPolygon>(downCast<OGRMultiPolygon*>(
        OGRGeometryFactory::createGeometry(wkbMultiPolygon)),
        [](OGRMultiPolygon* f) {OGRGeometryFactory::destroyGeometry(f);});
    this->data->addGeometry(geom->toPolygon());
  } else if (wkbFlatten(geom->getGeometryType()) ==
    OGRwkbGeometryType::wkbMultiPolygon) {
    this->data = std::shared_ptr<OGRMultiPolygon>(
        downCast<OGRMultiPolygon*>(geom->clone()),
        [](OGRMultiPolygon* f) {OGRGeometryFactory::destroyGeometry(f);});
  } else if (wkbFlatten(geom->getGeometryType()) ==
    OGRwkbGeometryType::wkbGeometryCollection) {
      data = std::shared_ptr<OGRMultiPolygon>(
        downCast<OGRMultiPolygon*>(
          OGRGeometryFactory::createGeometry(wkbMultiPolygon)),
            [](OGRMultiPolygon* f) {OGRGeometryFactory::destroyGeometry(f);});
  } else {
    throw std::invalid_argument(sstr(
        "Cells(const OGRGeometry*): Type of OGRGeometry* is " ,
        wkbFlatten(geom->getGeometryType()) , " instead of wkbPolygon(",
        OGRwkbGeometryType::wkbPolygon , ") or wkbMultiPolygon(",
        OGRwkbGeometryType::wkbMultiPolygon , ")"));
  }
}


Cells::Cells(const Cell& c) {
  data->addGeometry(c.get());
}

void Cells::operator*=(double b) {
  for (auto&& poly : *this) {
    poly *= b;
  }
}

void Cells::getGeometry(size_t i, Cell& cell) {
  cell = Cell(data->getGeometryRef(i), EmptyDestructor());
}

void Cells::getGeometry(size_t i, Cell& cell) const {
  cell = Cell(data->getGeometryRef(i), EmptyDestructor());
}

Cell Cells::getGeometry(size_t i) {
  return Cell(data->getGeometryRef(i));
}

const Cell Cells::getGeometry(size_t i) const {
  return Cell(data->getGeometryRef(i));
}

void Cells::setGeometry(size_t i, const Cell& cell) {
  auto n = this->size();
  if (i < n) {
    Cells cells;
    for (size_t j = 0; j < n; ++j) {
      cells.addGeometry((i == j) ? cell : this->getGeometry(j));
    }
    *this = cells;
    return;
  } else if (i != n) {
    for (size_t j = n; j < i; ++j) {
      this->addGeometry(Cell());
    }
  }
  this->addGeometry(cell);
}

void Cells::addGeometry(const Cell& c) {
  this->data->addGeometry(c.get());
}

void Cells::addRing(size_t i, const LinearRing& ring) {
  downCast<OGRPolygon*>(this->data->getGeometryRef(i))->addRing(ring.get());
}

size_t Cells::size() const {
  return isEmpty() ? 0 : data->getNumGeometries();
}

Cell Cells::getCell(size_t i) const {
  return Cell(data->getGeometryRef(i));
}

LinearRing Cells::getCellBorder(size_t i) const {
  return LinearRing(
      downCast<OGRPolygon*>(data->getGeometryRef(i))->getExteriorRing());
}

LinearRing Cells::getInteriorRing(size_t i_cell, size_t i_ring) const {
  return LinearRing(downCast<OGRPolygon*>(data->getGeometryRef(i_cell))
      ->getInteriorRing(i_ring));
}

bool Cells::isConvex() const {
  return (isEmpty() || data->getNumGeometries() != 1)? false :
    getCell(0).isConvex();
}

Cells Cells::Intersection(const Cells& c) const {
  auto inter = data->Intersection(c.get());
  if (wkbFlatten(inter->getGeometryType()) ==
        OGRwkbGeometryType::wkbPolygon ||
      wkbFlatten(inter->getGeometryType()) ==
        OGRwkbGeometryType::wkbMultiPolygon) {
    Cells cells(inter->toMultiPolygon());
    OGRGeometryFactory::destroyGeometry(inter);
    return cells;
  } else {
    OGRGeometryFactory::destroyGeometry(inter);
    return Cells();
  }
}

Cells Cells::Difference(const Cells& c) const {
  auto diff = data->Difference(c.get());
  Cells cells(diff);
  OGRGeometryFactory::destroyGeometry(diff);
  return cells;
}

Cells Cells::splitByLine(const LineString& line) const {
  // This trick could be faster using Polygonize
  return this->Difference(Buffer(line, 1e-8));
}

Cells Cells::splitByLine(const MultiLineString& lines) const {
  Cells cells{*this};
  for (auto&& line : lines) {
    cells = cells.splitByLine(line);
  }
  return cells;
}

Cells Cells::Intersection(const Cell& c) const {
  return Cells(c).Intersection(*this);
}

Cells Cells::Intersection(
    const Cell& cell, const Cell& c) {
  return Cells(cell).Intersection(c);
}

LineString Cells::getSemiLongCurve(const Point& point, double angle) const {
  return LineString({point,
    point.getPointFromAngle(angle, this->getMinSafeLength())});
}

LineString Cells::getStraightLongCurve(
    const Point& point, double angle) const {
  return LineString({
    point.getPointFromAngle(angle, this->getMinSafeLength()),
    point.getPointFromAngle(
      boost::math::constants::pi<double>() + angle, this->getMinSafeLength())});
}

MultiLineString Cells::getLinesInside(const LineString& line) const {
  return MultiLineString::Intersection(line, *this);
}

MultiLineString Cells::getLinesInside(const MultiLineString& lines) const {
  return lines.Intersection(*this);
}

Cells Cells::getCellsInside(const Cells& cell) const {
  return this->Intersection(cell);
}

bool Cells::isPointInBorder(const Point& p) const {
  return p.Touches(*this);
}

bool Cells::isPointIn(const Point& p) const {
  return p.Within(*this);
}

Cell Cells::getCellWherePoint(const Point& p) const {
  for (auto&& cell : *this) {
    if (p.Touches(cell) || p.Within(cell)) {
      return cell;
    }
  }
  return Cell();
}

LineString Cells::createLineUntilBorder(
    const f2c::types::Point& p, double ang) const {
  return this->getCellWherePoint(p).createLineUntilBorder(p, ang);
}


}  // namespace f2c::types

