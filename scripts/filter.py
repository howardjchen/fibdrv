#!/usr/bin/env python3
import subprocess
import numpy as np
import matplotlib.pyplot as plt

pool = []
filter_pool = []
k_ans = []
u_ans = []
k2u_ans = []


def find_best_mean(file, ans_list):
	with open(file) as row_data:
		for line in row_data:

			# for each row, split and add nums to pool
			row = line.split()
			for ele_str in row[1:]:
				pool.append(int(ele_str))

			# Calculate std for each pool
			data = np.array(pool)
			left = data.mean() - 2 * data.std()
			right = data.mean() + 2 * data.std()

			# Filter out and find mean average of 95% data
			for x in pool:
				if left <= x and x <= right:
					filter_pool.append(x)
			ans = np.array(filter_pool)
			ans_list.append(ans.mean())
			filter_pool.clear()
			pool.clear()

if __name__ == "__main__":
	find_best_mean('./k_data.txt', k_ans)
	find_best_mean('./u_data.txt', u_ans)
	find_best_mean('./k2u_data.txt', k2u_ans)

	with open('./k_data.txt') as data:
		line_number = len(data.readlines())

	with open('./data.txt', 'w') as f:
		for i in range(line_number):
			print(i, " ", round(k_ans[i], 2), " ", round(u_ans[i], 2)," ", round(k2u_ans[i], 2), file=f)