import datetime
import io
import logging
import random
# import mysql.connector
# from mysql.connector import errorcode
# from sqlalchemy import create_engine
import sys
import time
from math import sqrt

from PIL import Image
from django.views.decorators.csrf import csrf_exempt
from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework.status import (
    HTTP_200_OK)
from sklearn.cluster import KMeans

# Create your views here.

logger = logging.getLogger('iot-seggregator')
logging.basicConfig(level=logging.DEBUG)

container_map = {
    1: "Red",
    2: "Green",
    3: "Blue"
}


class Point:
    def __init__(self, coordinates):
        self.coordinates = coordinates


class Cluster:
    def __init__(self, center, points):
        self.center = center
        self.points = points


class KMeans:

    def __init__(self, n_clusters, min_diff=1):
        self.n_clusters = n_clusters
        self.min_diff = min_diff

    def calculate_center(self, points):
        n_dim = len(points[0].coordinates)
        vals = [0.0 for i in range(n_dim)]
        for p in points:
            for i in range(n_dim):
                vals[i] += p.coordinates[i]
        coords = [(v / len(points)) for v in vals]
        return Point(coords)

    def assign_points(self, clusters, points):
        plists = [[] for i in range(self.n_clusters)]

        for p in points:
            smallest_distance = float('inf')

            for i in range(self.n_clusters):
                distance = euclidean(p, clusters[i].center)
                if distance < smallest_distance:
                    smallest_distance = distance
                    idx = i

            plists[idx].append(p)

        return plists

    def fit(self, points):
        clusters = [Cluster(center=p, points=[p]) for p in random.sample(points, self.n_clusters)]

        while True:

            plists = self.assign_points(clusters, points)

            diff = 0

            for i in range(self.n_clusters):
                if not plists[i]:
                    continue
                old = clusters[i]
                center = self.calculate_center(plists[i])
                new = Cluster(center, plists[i])
                clusters[i] = new
                diff = max(diff, euclidean(old.center, new.center))

            if diff < self.min_diff:
                break

        return clusters


def euclidean(p, q):
    n_dim = len(p.coordinates)
    return sqrt(sum([
        (p.coordinates[i] - q.coordinates[i]) ** 2 for i in range(n_dim)
    ]))


def get_points(image_path):
    img = Image.open(image_path)
    img.thumbnail((200, 400))
    img = img.convert("RGB")
    w, h = img.size

    points = []
    for count, color in img.getcolors(w * h):
        for _ in range(count):
            points.append(Point(color))

    return points


def rgb_to_hex(rgb):
    return '#%s' % ''.join(('%02x' % p for p in rgb))


def get_colors(filename, n_colors=3):
    points = get_points(filename)
    clusters = KMeans(n_clusters=n_colors).fit(points)
    clusters.sort(key=lambda c: len(c.points), reverse=True)
    rgbs = []
    for c in clusters:
        rgbs.append(c.center.coordinates)
    return rgbs


@csrf_exempt
@api_view(["POST"])
def process_image(request):
    # logger.debug(f"received body : {request.body}")
    db_name = "sorting_inventory"
    table_name = "packages_inventory"
    max_allowed_color = 5
    buff = io.BytesIO(request.body)
    color_detected = get_colors(buff, n_colors=5)
    final_color = max([color.index(max(color)) for color in color_detected])
    color_code = final_color + 1
    logger.info(f"{final_color} - {color_code}")
    # flag = insert_records(db_name, table_name, container_map[color_code])
    return Response(color_code, status=HTTP_200_OK)


def insert_records(db_name, table, detected_color):
    return_flag = True
    print(db_name, table)
    cnx = mysql.connector.connect(
        host="inventory-records.c6xbsgoq927m.us-east-1.rds.amazonaws.com",
        database=db_name,
        user="admin",
        password="IoT-Segragator")

    cursor = cnx.cursor()
    ts = time.time()
    timestamp = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
    logger.info(f'timestamp of the identified image {timestamp}')
    try:
        query_builder = f"INSERT INTO {table} (time, container_id, count)  VALUES (\"{timestamp}\", {container_map[detected_color]}, {1});"
        logger.info(f"insert query - {query_builder}")
        cursor.execute(query_builder)
        cnx.commit()
    except:
        logger.error(f"rolling back the query - {sys.exc_info()}")
        cnx.rollback()
        return_flag = False
    cnx.close()
    return return_flag
